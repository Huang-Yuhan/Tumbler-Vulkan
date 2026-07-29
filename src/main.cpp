#include "Assets/MeshLoader.h"
#include "Core/Platform/AppWindow.h"
#include "Core/Utils/Log.h"
#include "Gfx/CommandManager.h"
#include "Gfx/DeletionQueue.h"
#include "Gfx/PipelineBuilder.h"
#include "Gfx/Swapchain.h"
#include "Gfx/VulkanDevice.h"
#include "Render/Camera.h"
#include "Render/Nanite/NaniteBuilder.h"
#include "UI/ImGuiLayer.h"

#include <imgui.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>

#include <GLFW/glfw3.h>

#include <cstring>

namespace {

enum class RenderMode {
    Shaded,
    Wireframe,
    ShadedWireframe,
};

struct MeshGpuState {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkBuffer indexBuffer  = VK_NULL_HANDLE;
    VkBuffer colorBuffer  = VK_NULL_HANDLE;
    VmaAllocation vertAlloc = VK_NULL_HANDLE;
    VmaAllocation idxAlloc  = VK_NULL_HANDLE;
    VmaAllocation colAlloc  = VK_NULL_HANDLE;
    VkPipelineLayout pipeLayout    = VK_NULL_HANDLE;
    VkPipeline       pipelineFill  = VK_NULL_HANDLE;
    VkPipeline       pipelineLine  = VK_NULL_HANDLE;
    uint32_t         indexCount    = 0;
};

// Push constant: viewProj + wireframe flag (matches cluster_vis.slang)
struct PushData {
    glm::mat4 viewProj;
    uint32_t  wireframe;
};
static_assert(sizeof(PushData) >= sizeof(glm::mat4) + sizeof(uint32_t));

// Generate a maximally distinct color from cluster ID.
// Uses golden-ratio conjugate to scatter hues so adjacent cluster IDs
// are never close in color, plus varies saturation and value.
glm::vec3 ClusterColor(int32_t clusterId, int32_t numClusters) {
    if (numClusters <= 0) return glm::vec3(0.5f);

    constexpr float kGoldenRatioConjugate = 0.618033988749895f;
    float hue = std::fmod(float(clusterId) * kGoldenRatioConjugate, 1.0f);

    // Vary saturation and value by cluster parity to add contrast
    float saturation = (clusterId & 1) ? 0.85f : 0.65f;
    float value      = (clusterId & 2) ? 0.90f : 0.70f;

    // HSV→RGB
    float c = value * saturation;
    float x = c * (1.0f - std::abs(std::fmod(hue * 6.0f, 2.0f) - 1.0f));
    float m = value - c;

    float r, g, b;
    if      (hue < 1.0f / 6.0f) { r = c; g = x; b = 0; }
    else if (hue < 2.0f / 6.0f) { r = x; g = c; b = 0; }
    else if (hue < 3.0f / 6.0f) { r = 0; g = c; b = x; }
    else if (hue < 4.0f / 6.0f) { r = 0; g = x; b = c; }
    else if (hue < 5.0f / 6.0f) { r = x; g = 0; b = c; }
    else                         { r = c; g = 0; b = x; }

    return glm::vec3(r + m, g + m, b + m);
}

bool UploadMeshForClusterVis(VkDevice device, VmaAllocator allocator,
                              Tumbler::CommandManager& cmdManager,
                              Tumbler::DeletionQueue& dq,
                              const Tumbler::MeshData& mesh,
                              const std::vector<int32_t>& part,
                              int32_t numClusters,
                              VkFormat colorFormat,
                              MeshGpuState& state) {
    if (mesh.vertices.empty() || mesh.indices.empty()) return false;

    // ---- Build per-vertex cluster colors ----
    std::vector<glm::vec3> colors(mesh.vertices.size(), glm::vec3(0.0f));
    int32_t numTriangles = static_cast<int32_t>(mesh.indices.size() / 3);
    for (int32_t t = 0; t < numTriangles; ++t) {
        int32_t cid = (t < static_cast<int32_t>(part.size())) ? part[t] : 0;
        glm::vec3 color = ClusterColor(cid, numClusters);
        for (int32_t k = 0; k < 3; ++k) {
            uint32_t vi = mesh.indices[t * 3 + k];
            if (vi < colors.size()) colors[vi] = color;
        }
    }

    // ---- Build interleaved vertex buffer: position(12B) + color(12B) = 24B ----
    struct VertexPC {
        float px, py, pz;
        float cr, cg, cb;
    };
    std::vector<VertexPC> vertData(mesh.vertices.size());
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        vertData[i] = {
            mesh.vertices[i].pos.x, mesh.vertices[i].pos.y, mesh.vertices[i].pos.z,
            colors[i].x, colors[i].y, colors[i].z,
        };
    }

    auto UploadBuffer = [&](const void* data, VkDeviceSize size,
                            VkBufferUsageFlags usage,
                            VkBuffer& buffer, VmaAllocation& alloc) {
        VkBufferCreateInfo bufInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = size,
            .usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        };
        VmaAllocationCreateInfo allocInfo{ .usage = VMA_MEMORY_USAGE_GPU_ONLY };
        vmaCreateBuffer(allocator, &bufInfo, &allocInfo, &buffer, &alloc, nullptr);

        VkBuffer staging;
        VmaAllocation stagingAlloc;
        VkBufferCreateInfo stageInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = size,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        };
        VmaAllocationCreateInfo stageAllocInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };
        vmaCreateBuffer(allocator, &stageInfo, &stageAllocInfo,
                        &staging, &stagingAlloc, nullptr);

        void* mapped;
        vmaMapMemory(allocator, stagingAlloc, &mapped);
        std::memcpy(mapped, data, size);
        vmaUnmapMemory(allocator, stagingAlloc);

        cmdManager.ImmediateSubmit([&](VkCommandBuffer cmd) {
            VkBufferCopy region{ .size = size };
            vkCmdCopyBuffer(cmd, staging, buffer, 1, &region);
        });

        dq.Enqueue([allocator, staging, stagingAlloc]() {
            vmaDestroyBuffer(allocator, staging, stagingAlloc);
        });
    };

    UploadBuffer(vertData.data(), vertData.size() * sizeof(VertexPC),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 state.vertexBuffer, state.vertAlloc);
    UploadBuffer(mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 state.indexBuffer, state.idxAlloc);
    state.indexCount = static_cast<uint32_t>(mesh.indices.size());

    // ---- Pipeline layout: push constant (mat4 + uint for wireframe flag) ----
    VkPushConstantRange pushRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size       = sizeof(PushData),
    };
    VkPipelineLayoutCreateInfo pipeLayoutInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 0,
        .pSetLayouts            = nullptr,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &pushRange,
    };
    vkCreatePipelineLayout(device, &pipeLayoutInfo, nullptr, &state.pipeLayout);

    // ---- Graphics pipeline with vertex attributes ----
    // Interleaved: position(12B) + color(12B) = 24B stride
    VkVertexInputBindingDescription bindDesc{
        .binding   = 0,
        .stride    = 24,  // float3 pos + float3 color
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription attribDescs[2] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0  },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 12 },
    };
    std::span<VkVertexInputBindingDescription> bindSpan(&bindDesc, 1);
    std::span<VkVertexInputAttributeDescription> attrSpan(attribDescs, 2);

    std::vector<VkFormat> formats = { colorFormat };
    std::span<VkFormat> colorSpan(formats);
    Tumbler::GraphicsPipelineBuilder pipeBuilder{
        .layout         = state.pipeLayout,
        .vertPath       = SHADER_DIR "/cluster_vis_vert.spv",
        .fragPath       = SHADER_DIR "/cluster_vis_frag.spv",
        .colorFormats   = colorSpan,
        .depthFormat    = VK_FORMAT_D32_SFLOAT,
        .cullMode       = VK_CULL_MODE_BACK_BIT,
        .vertexBindings = bindSpan,
        .vertexAttribs  = attrSpan,
    };

    // Fill pipeline
    auto fillPipe = pipeBuilder.Build(device);
    if (!fillPipe) return false;
    state.pipelineFill = *fillPipe;

    // Line (wireframe) pipeline
    pipeBuilder.polygonMode       = VK_POLYGON_MODE_LINE;
    pipeBuilder.depthBiasEnable   = VK_TRUE;
    pipeBuilder.depthBiasConstant = -1.0f;
    pipeBuilder.depthBiasSlope    = -1.0f;
    auto linePipe = pipeBuilder.Build(device);
    if (!linePipe) return false;
    state.pipelineLine = *linePipe;

    LOG_INFO("Mesh upload: {} vertices, {} triangles, {} clusters",
             mesh.vertices.size(), numTriangles, numClusters);
    return true;
}

void DestroyMeshGpuState(VkDevice device, VmaAllocator allocator,
                          Tumbler::DeletionQueue& dq,
                          MeshGpuState& state) {
    if (state.pipelineFill) dq.Enqueue([device, p = state.pipelineFill]() { vkDestroyPipeline(device, p, nullptr); });
    if (state.pipelineLine) dq.Enqueue([device, p = state.pipelineLine]() { vkDestroyPipeline(device, p, nullptr); });
    if (state.pipeLayout)  dq.Enqueue([device, l = state.pipeLayout]() { vkDestroyPipelineLayout(device, l, nullptr); });
    if (state.vertexBuffer) dq.Enqueue([allocator, b = state.vertexBuffer, a = state.vertAlloc]()
        { vmaDestroyBuffer(allocator, b, a); });
    if (state.indexBuffer) dq.Enqueue([allocator, b = state.indexBuffer, a = state.idxAlloc]()
        { vmaDestroyBuffer(allocator, b, a); });
}

} // anonymous namespace

int main(int argc, char** argv) {
    using namespace Tumbler;

    Log::Init();

    // ---- Load mesh ----
    const char* meshPath = (argc > 1) ? argv[1] : ASSET_DIR "/models/bunny.obj";
    auto mesh = LoadObj(meshPath);
    if (!mesh) {
        LOG_ERROR("Failed to load mesh: {}", meshPath);
        return 1;
    }
    // Bunny is ~0.2 units wide - scale up for visibility
    for (auto& v : mesh->vertices) v.pos *= 10.0f;
    LOG_INFO("Mesh loaded: {} vertices, {} triangles",
             mesh->vertices.size(), mesh->indices.size() / 3);

    // ---- Nanite partition ----
    auto naniteData = Nanite::NaniteBuilder().Build(*mesh);
    if (!naniteData) {
        LOG_ERROR("Nanite build failed");
        return 1;
    }
    auto& clusters = naniteData->clusterDag.GetClusters();
    const auto& part = naniteData->clusterDag.GetPart();
    int32_t numClusters = static_cast<int32_t>(clusters.size());
    LOG_INFO("Nanite partition: {} clusters", numClusters);

    // ---- Window ----
    AppWindow window;
    if (!window.Init(1920, 1080, "Tumbler — Cluster Visualization")) {
        LOG_ERROR("Window init failed");
        return 1;
    }

    // ---- Vulkan ----
    VulkanDevice device;
    if (!device.CreateInstance()) return 1;
    auto surface = window.CreateSurface(device.GetInstance());
    if (!surface) return 1;
    if (!device.CompleteInit(*surface)) return 1;

    // ---- VMA ----
    VmaAllocatorCreateInfo vmaInfo{
        .physicalDevice   = device.GetPhysicalDevice(),
        .device           = device.GetDevice(),
        .instance         = device.GetInstance(),
        .vulkanApiVersion = VK_API_VERSION_1_4,
    };
    VmaAllocator vmaAllocator;
    vmaCreateAllocator(&vmaInfo, &vmaAllocator);

    // ---- Swapchain ----
    Swapchain swapchain;
    int fbW, fbH;
    window.GetFramebufferSize(&fbW, &fbH);
    if (!swapchain.Init(device, *surface, fbW, fbH)) return 1;

    // ---- Deletion Queue ----
    DeletionQueue deletionQueue;
    deletionQueue.Init(device.GetDevice());

    // ---- Command Manager ----
    CommandManager cmdManager;
    if (!cmdManager.Init(device.GetDevice(), device.GetQueueFamilies().graphics)) return 1;

    // ---- Camera ----
    Camera camera;
    camera.SetPerspective(glm::radians(60.0f), 0.1f, 1000.0f);
    camera.SetTarget(glm::vec3(0.0f));
    camera.SetDistance(5.0f);
    camera.Orbit(0.0f, glm::radians(20.0f));  // look slightly down

    // ---- ImGui ----
    ImGuiLayer imgui;
    {
        ImGuiLayer::Config imguiConfig{
            .window         = window.GetHandle(),
            .instance       = device.GetInstance(),
            .physicalDevice = device.GetPhysicalDevice(),
            .device         = device.GetDevice(),
            .queueFamily    = device.GetQueueFamilies().graphics,
            .queue          = device.GetGraphicsQueue(),
            .minImageCount  = swapchain.GetImageCount(),
            .colorFormat    = swapchain.GetFormat(),
            .depthFormat    = VK_FORMAT_D32_SFLOAT,
        };
        if (!imgui.Init(imguiConfig)) {
            LOG_ERROR("ImGui init failed");
            return 1;
        }
    }

    // ---- Upload mesh with cluster colors ----
    MeshGpuState meshGpu;
    if (!UploadMeshForClusterVis(device.GetDevice(), vmaAllocator,
                                  cmdManager, deletionQueue, *mesh,
                                  part, numClusters,
                                  swapchain.GetFormat(), meshGpu)) {
        LOG_ERROR("Mesh GPU upload failed");
        return 1;
    }

    // ---- Acquire fence ----
    VkFenceCreateInfo fenceInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence acquireFence;
    vkCreateFence(device.GetDevice(), &fenceInfo, nullptr, &acquireFence);

    // ---- Main Loop ----
    LOG_INFO("Entering main loop");
    while (!window.ShouldClose()) {
        window.PollEvents();

        // Handle pending resize (from Present or previous Acquire)
        if (swapchain.NeedsRecreate()) {
            window.GetFramebufferSize(&fbW, &fbH);
            if (fbW > 0 && fbH > 0) {
                swapchain.Recreate(fbW, fbH);
                imgui.OnSwapchainRecreate(swapchain.GetImageCount());
            }
            // Skip this frame — no valid swapchain to render to
            continue;
        }

        deletionQueue.Flush();

        uint32_t imageIndex = 0;
        VkResult acquireResult = swapchain.AcquireNextImage(&imageIndex, VK_NULL_HANDLE, acquireFence);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR) {
            // Swapchain out of date — recreate immediately, don't defer
            window.GetFramebufferSize(&fbW, &fbH);
            if (fbW > 0 && fbH > 0) {
                swapchain.Recreate(fbW, fbH);
                imgui.OnSwapchainRecreate(swapchain.GetImageCount());
            }
            continue;
        }
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) break;

        vkWaitForFences(device.GetDevice(), 1, &acquireFence, VK_TRUE, UINT64_MAX);
        vkResetFences(device.GetDevice(), 1, &acquireFence);

        // ---- ImGui begin (must be before any ImGui::GetIO usage) ----
        imgui.BeginFrame();

        // ---- Camera (uses ImGui::GetIO().DeltaTime) ----
        float aspect = static_cast<float>(swapchain.GetExtent().width) /
                       static_cast<float>(swapchain.GetExtent().height);
        camera.SetAspectRatio(aspect);
        camera.Orbit(ImGui::GetIO().DeltaTime * 0.3f, 0.0f);  // auto-rotate

        // Mouse wheel zoom (when not hovering ImGui windows)
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
            camera.Zoom(wheel * 2.0f);
        }

        glm::mat4 viewProj = camera.GetViewProjection();

        // ---- ImGui UI ----
        static RenderMode renderMode = RenderMode::ShadedWireframe;
        {
            ImGui::Begin("Render Settings");
            const char* modes[] = {"Shaded", "Wireframe", "Shaded Wireframe"};
            int currentMode = static_cast<int>(renderMode);
            if (ImGui::Combo("Mode", &currentMode, modes, 3)) {
                renderMode = static_cast<RenderMode>(currentMode);
            }
            ImGui::Separator();
            float dist = camera.GetDistance();
            if (ImGui::SliderFloat("Distance", &dist, 0.5f, 100.0f, "%.1f")) {
                camera.SetDistance(dist);
            }
            ImGui::Text("Yaw: %.1f  Pitch: %.1f",
                        glm::degrees(camera.GetYaw()),
                        glm::degrees(camera.GetPitch()));
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::Text("Clusters: %d", numClusters);
            ImGui::End();
        }

        // ---- Record ----
        VkCommandBuffer cmd = cmdManager.Allocate();
        VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(cmd, &beginInfo);

        cmdManager.TransitionLayout(cmd, swapchain.GetImage(imageIndex),
                                     VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        cmdManager.TransitionLayout(cmd, swapchain.GetDepthImage(),
                                     VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                     VK_IMAGE_ASPECT_DEPTH_BIT);

        VkRenderingAttachmentInfo colorAttachment{
            .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView   = swapchain.GetImageView(imageIndex),
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue  = { .color = {{ 0.1f, 0.1f, 0.15f, 1.0f }} },
        };
        VkRenderingAttachmentInfo depthAttachment{
            .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView   = swapchain.GetDepthView(),
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue  = { .depthStencil = { 0.0f, 0 } },
        };

        VkRenderingInfo renderInfo{
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea           = { { 0, 0 }, swapchain.GetExtent() },
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colorAttachment,
            .pDepthAttachment     = &depthAttachment,
        };
        vkCmdBeginRendering(cmd, &renderInfo);

        // ---- Draw mesh ----
        auto DrawMesh = [&](VkPipeline pipeline, bool wireframe) {
            PushData pushData{ viewProj, wireframe ? 1u : 0u };
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdPushConstants(cmd, meshGpu.pipeLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(PushData), &pushData);

            VkViewport viewport{
                .x        = 0.0f,
                .y        = 0.0f,
                .width    = static_cast<float>(swapchain.GetExtent().width),
                .height   = static_cast<float>(swapchain.GetExtent().height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
            };
            VkRect2D scissor{ .offset = { 0, 0 }, .extent = swapchain.GetExtent() };
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            VkDeviceSize vtxOffset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &meshGpu.vertexBuffer, &vtxOffset);
            vkCmdBindIndexBuffer(cmd, meshGpu.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, meshGpu.indexCount, 1, 0, 0, 0);
        };

        switch (renderMode) {
        case RenderMode::Shaded:
            DrawMesh(meshGpu.pipelineFill, false);
            break;
        case RenderMode::Wireframe:
            DrawMesh(meshGpu.pipelineLine, true);
            break;
        case RenderMode::ShadedWireframe:
            DrawMesh(meshGpu.pipelineFill, false);
            DrawMesh(meshGpu.pipelineLine, true);
            break;
        }

        // ---- ImGui overlay ----
        imgui.EndFrame(cmd);

        vkCmdEndRendering(cmd);

        cmdManager.TransitionLayout(cmd, swapchain.GetImage(imageIndex),
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        vkEndCommandBuffer(cmd);

        // ---- Submit ----
        uint64_t signalValue = deletionQueue.AdvanceSubmitCounter();
        VkTimelineSemaphoreSubmitInfo timelineInfo{
            .sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
            .signalSemaphoreValueCount = 1,
            .pSignalSemaphoreValues    = &signalValue,
        };
        VkSemaphore timelineSem = deletionQueue.GetTimelineSemaphore();
        VkSubmitInfo submitInfo{
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                = &timelineInfo,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &cmd,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores    = &timelineSem,
        };
        vkQueueSubmit(device.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(device.GetGraphicsQueue());

        vkFreeCommandBuffers(device.GetDevice(), cmdManager.GetPool(), 1, &cmd);

        swapchain.Present(device.GetPresentQueue(), imageIndex, VK_NULL_HANDLE);
    }

    // ---- Shutdown ----
    imgui.Shutdown();
    vkDeviceWaitIdle(device.GetDevice());
    vkDestroyFence(device.GetDevice(), acquireFence, nullptr);
    DestroyMeshGpuState(device.GetDevice(), vmaAllocator, deletionQueue, meshGpu);
    deletionQueue.Shutdown();
    cmdManager.Shutdown();
    swapchain.Shutdown();
    vmaDestroyAllocator(vmaAllocator);
    vkDestroySurfaceKHR(device.GetInstance(), *surface, nullptr);
    device.Shutdown();
    window.Shutdown();
    Log::Shutdown();
    return 0;
}
