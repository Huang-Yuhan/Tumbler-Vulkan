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

using namespace Tumbler;
using namespace Tumbler::Nanite;

enum class RenderMode {
    Shaded,
    Wireframe,
    ShadedWireframe,
};

struct ClusterGpuState {
    VkBuffer         clusterMeta     = VK_NULL_HANDLE;  // StructuredBuffer<ClusterInfo>
    VkBuffer         indexBuffer     = VK_NULL_HANDLE;  // StructuredBuffer<uint>
    VkBuffer         positionBuffer  = VK_NULL_HANDLE;  // StructuredBuffer<float> (xyz per vertex)
    VmaAllocation    metaAlloc       = VK_NULL_HANDLE;
    VmaAllocation    idxAlloc        = VK_NULL_HANDLE;
    VmaAllocation    posAlloc        = VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout  = VK_NULL_HANDLE;
    VkDescriptorPool      descPool   = VK_NULL_HANDLE;
    VkDescriptorSet       descSet    = VK_NULL_HANDLE;
    VkPipelineLayout      pipeLayout = VK_NULL_HANDLE;
    VkPipeline            pipelineFill = VK_NULL_HANDLE;
    VkPipeline            pipelineLine = VK_NULL_HANDLE;
    uint32_t              vertexCount  = 0;  // per-cluster (= kClusterTriangleCount * 3)
    uint32_t              numClusters  = 0;
};

// Push constant: matches cluster_draw.slang
struct PushData {
    glm::mat4 viewProj;
    uint32_t  wireframe;
};
static_assert(sizeof(PushData) >= sizeof(glm::mat4) + sizeof(uint32_t));

// Precompute a maximally distinct color palette for N clusters
std::vector<glm::vec4> BuildClusterColors(int32_t numClusters) {
    std::vector<glm::vec4> colors(numClusters);
    constexpr float kGolden = 0.618033988749895f;
    for (int32_t i = 0; i < numClusters; ++i) {
        float hue = std::fmod(float(i) * kGolden, 1.0f);
        float sat = (i & 1) ? 0.85f : 0.65f;
        float val = (i & 2) ? 0.90f : 0.70f;

        float c = val * sat;
        float x = c * (1.0f - std::abs(std::fmod(hue * 6.0f, 2.0f) - 1.0f));
        float m = val - c;
        float r, g, b;
        if      (hue < 1.0f/6.0f) { r=c; g=x; b=0; }
        else if (hue < 2.0f/6.0f) { r=x; g=c; b=0; }
        else if (hue < 3.0f/6.0f) { r=0; g=c; b=x; }
        else if (hue < 4.0f/6.0f) { r=0; g=x; b=c; }
        else if (hue < 5.0f/6.0f) { r=x; g=0; b=c; }
        else                       { r=c; g=0; b=x; }
        colors[i] = glm::vec4(r + m, g + m, b + m, 1.0f);
    }
    return colors;
}

bool UploadClusterData(VkDevice device, VmaAllocator allocator,
                       CommandManager& cmdManager, DeletionQueue& dq,
                       const std::vector<Cluster>& clusters,
                       VkFormat colorFormat,
                       ClusterGpuState& state) {
    if (clusters.empty()) return false;

    state.numClusters = static_cast<uint32_t>(clusters.size());
    state.vertexCount = kClusterTriangleCount * 3;  // 384

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
        vmaCreateBuffer(allocator, &stageInfo, &stageAllocInfo, &staging, &stagingAlloc, nullptr);

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

    // ---- 1. Build flat position buffer: xyz per unique vertex ----
    // Iterate clusters, extract positions from VertexData, dedup by offset tracking
    std::vector<float> allPositions;      // xyz per vertex
    std::vector<uint32_t> allIndices;     // packed local indices
    std::vector<glm::vec4> clusterColors = BuildClusterColors(state.numClusters);

    // Cluster metadata struct (matches shader's ClusterInfo)
    struct CpuClusterInfo {
        uint32_t indexOffset;
        uint32_t vertexOffset;
        uint32_t indexCount;
        uint32_t pad;
        float    color[4];
    };
    std::vector<CpuClusterInfo> meta(state.numClusters);

    uint32_t curIndexOffset  = 0;
    uint32_t curVertexOffset = 0;

    for (uint32_t c = 0; c < state.numClusters; ++c) {
        const auto& cluster = clusters[c];
        uint32_t numVerts = cluster.NumVertices;
        uint32_t numIdx   = static_cast<uint32_t>(cluster.indices.size());

        // Store positions: VertexData[i] is 32 bytes (Vertex struct), pos is first 12 bytes
        for (uint32_t v = 0; v < numVerts; ++v) {
            const float* src = reinterpret_cast<const float*>(cluster.VertexData.data() + v * sizeof(Vertex));
            allPositions.push_back(src[0]);  // pos.x
            allPositions.push_back(src[1]);  // pos.y
            allPositions.push_back(src[2]);  // pos.z
        }

        // Store indices (local indices already remapped to local vertex range)
        allIndices.insert(allIndices.end(), cluster.indices.begin(), cluster.indices.end());

        meta[c] = {
            curIndexOffset,
            curVertexOffset,
            numIdx,
            0,
            { clusterColors[c].x, clusterColors[c].y, clusterColors[c].z, clusterColors[c].w }
        };

        curIndexOffset  += numIdx;
        curVertexOffset += numVerts;
    }

    // ---- 2. Upload buffers ----
    UploadBuffer(meta.data(), meta.size() * sizeof(CpuClusterInfo),
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                 state.clusterMeta, state.metaAlloc);
    UploadBuffer(allIndices.data(), allIndices.size() * sizeof(uint32_t),
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                 state.indexBuffer, state.idxAlloc);
    UploadBuffer(allPositions.data(), allPositions.size() * sizeof(float),
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                 state.positionBuffer, state.posAlloc);

    // ---- 3. Descriptor set layout ----
    VkDescriptorSetLayoutBinding bindings[3] = {
        { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT },
        { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT },
        { .binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT },
    };
    VkDescriptorSetLayoutCreateInfo setLayoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 3,
        .pBindings    = bindings,
    };
    vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &state.setLayout);

    // ---- 4. Descriptor pool + set ----
    VkDescriptorPoolSize poolSize{
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 3,
    };
    VkDescriptorPoolCreateInfo poolInfo{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets       = 1,
        .poolSizeCount = 1,
        .pPoolSizes    = &poolSize,
    };
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &state.descPool);

    VkDescriptorSetAllocateInfo setAlloc{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = state.descPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &state.setLayout,
    };
    vkAllocateDescriptorSets(device, &setAlloc, &state.descSet);

    // ---- 5. Write descriptor set ----
    VkDescriptorBufferInfo bufInfos[3] = {
        { .buffer = state.clusterMeta,    .offset = 0, .range = VK_WHOLE_SIZE },
        { .buffer = state.indexBuffer,    .offset = 0, .range = VK_WHOLE_SIZE },
        { .buffer = state.positionBuffer, .offset = 0, .range = VK_WHOLE_SIZE },
    };
    VkWriteDescriptorSet writes[3];
    for (int i = 0; i < 3; ++i) {
        writes[i] = {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = state.descSet,
            .dstBinding      = static_cast<uint32_t>(i),
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo     = &bufInfos[i],
        };
    }
    vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);

    // ---- 6. Pipeline layout ----
    VkPushConstantRange pushRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size       = sizeof(PushData),
    };
    VkPipelineLayoutCreateInfo pipeLayoutInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &state.setLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &pushRange,
    };
    vkCreatePipelineLayout(device, &pipeLayoutInfo, nullptr, &state.pipeLayout);

    // ---- 7. Graphics pipeline (no vertex input bindings — GPU-driven) ----
    std::vector<VkFormat> formats = { colorFormat };
    std::span<VkFormat> colorSpan(formats);
    std::span<VkVertexInputBindingDescription> emptyBindings;
    std::span<VkVertexInputAttributeDescription> emptyAttribs;

    GraphicsPipelineBuilder pipeBuilder{
        .layout         = state.pipeLayout,
        .vertPath       = SHADER_DIR "/cluster_draw_vert.spv",
        .fragPath       = SHADER_DIR "/cluster_draw_frag.spv",
        .colorFormats   = colorSpan,
        .depthFormat    = VK_FORMAT_D32_SFLOAT,
        .cullMode       = VK_CULL_MODE_BACK_BIT,
        .vertexBindings = emptyBindings,
        .vertexAttribs  = emptyAttribs,
    };

    auto fillPipe = pipeBuilder.Build(device);
    if (!fillPipe) return false;
    state.pipelineFill = *fillPipe;

    pipeBuilder.polygonMode       = VK_POLYGON_MODE_LINE;
    pipeBuilder.depthBiasEnable   = VK_TRUE;
    pipeBuilder.depthBiasConstant = -1.0f;
    pipeBuilder.depthBiasSlope    = -1.0f;
    auto linePipe = pipeBuilder.Build(device);
    if (!linePipe) return false;
    state.pipelineLine = *linePipe;

    LOG_INFO("Cluster upload: {} clusters, {} verts, {} indices",
             state.numClusters, allPositions.size() / 3, allIndices.size());
    return true;
}

void DestroyClusterGpuState(VkDevice device, VmaAllocator allocator,
                            DeletionQueue& dq, ClusterGpuState& state) {
    if (state.pipelineFill) dq.Enqueue([d = device, p = state.pipelineFill]() { vkDestroyPipeline(d, p, nullptr); });
    if (state.pipelineLine) dq.Enqueue([d = device, p = state.pipelineLine]() { vkDestroyPipeline(d, p, nullptr); });
    if (state.pipeLayout)   dq.Enqueue([d = device, l = state.pipeLayout]()  { vkDestroyPipelineLayout(d, l, nullptr); });
    if (state.descPool)     dq.Enqueue([d = device, p = state.descPool]()    { vkDestroyDescriptorPool(d, p, nullptr); });
    if (state.setLayout)    dq.Enqueue([d = device, l = state.setLayout]()   { vkDestroyDescriptorSetLayout(d, l, nullptr); });
    if (state.clusterMeta)  dq.Enqueue([a = allocator, b = state.clusterMeta,  al = state.metaAlloc]() { vmaDestroyBuffer(a, b, al); });
    if (state.indexBuffer)  dq.Enqueue([a = allocator, b = state.indexBuffer, al = state.idxAlloc]()  { vmaDestroyBuffer(a, b, al); });
    if (state.positionBuffer) dq.Enqueue([a = allocator, b = state.positionBuffer, al = state.posAlloc](){ vmaDestroyBuffer(a, b, al); });
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
    camera.Orbit(0.0f, glm::radians(20.0f));

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

    // ---- Upload cluster data ----
    ClusterGpuState clusterGpu;
    if (!UploadClusterData(device.GetDevice(), vmaAllocator,
                           cmdManager, deletionQueue, clusters,
                           swapchain.GetFormat(), clusterGpu)) {
        LOG_ERROR("Cluster GPU upload failed");
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

        if (swapchain.NeedsRecreate()) {
            window.GetFramebufferSize(&fbW, &fbH);
            if (fbW > 0 && fbH > 0) {
                swapchain.Recreate(fbW, fbH);
                imgui.OnSwapchainRecreate(swapchain.GetImageCount());
            }
            continue;
        }

        deletionQueue.Flush();

        uint32_t imageIndex = 0;
        VkResult acquireResult = swapchain.AcquireNextImage(&imageIndex, VK_NULL_HANDLE, acquireFence);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR) {
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

        // ---- ImGui begin ----
        imgui.BeginFrame();

        // ---- Camera ----
        float aspect = static_cast<float>(swapchain.GetExtent().width) /
                       static_cast<float>(swapchain.GetExtent().height);
        camera.SetAspectRatio(aspect);
        camera.Orbit(ImGui::GetIO().DeltaTime * 0.3f, 0.0f);

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

        // ---- Draw clusters (instanced) ----
        auto DrawClusters = [&](VkPipeline pipeline, bool wireframe) {
            PushData pushData{ viewProj, wireframe ? 1u : 0u };
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    clusterGpu.pipeLayout, 0, 1,
                                    &clusterGpu.descSet, 0, nullptr);
            vkCmdPushConstants(cmd, clusterGpu.pipeLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(PushData), &pushData);

            VkViewport viewport{
                .x        = 0.0f, .y = 0.0f,
                .width    = static_cast<float>(swapchain.GetExtent().width),
                .height   = static_cast<float>(swapchain.GetExtent().height),
                .minDepth = 0.0f, .maxDepth = 1.0f,
            };
            VkRect2D scissor{ .offset = { 0, 0 }, .extent = swapchain.GetExtent() };
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            // 384 vertices per instance (= 128 triangles * 3), N instances
            vkCmdDraw(cmd, clusterGpu.vertexCount, clusterGpu.numClusters, 0, 0);
        };

        switch (renderMode) {
        case RenderMode::Shaded:
            DrawClusters(clusterGpu.pipelineFill, false);
            break;
        case RenderMode::Wireframe:
            DrawClusters(clusterGpu.pipelineLine, true);
            break;
        case RenderMode::ShadedWireframe:
            DrawClusters(clusterGpu.pipelineFill, false);
            DrawClusters(clusterGpu.pipelineLine, true);
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
    DestroyClusterGpuState(device.GetDevice(), vmaAllocator, deletionQueue, clusterGpu);
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
