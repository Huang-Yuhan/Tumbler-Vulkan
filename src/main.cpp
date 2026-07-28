#include "Assets/MeshLoader.h"
#include "Core/Platform/AppWindow.h"
#include "Core/Utils/Log.h"
#include "Gfx/CommandManager.h"
#include "Gfx/DeletionQueue.h"
#include "Gfx/PipelineBuilder.h"
#include "Gfx/Swapchain.h"
#include "Gfx/VulkanDevice.h"
#include "Render/Nanite/NaniteBuilder.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>

#include <cstring>

namespace {

// ---- Nanite GPU resources ----
struct NaniteGpuBuffers {
    VkBuffer positionBuffer = VK_NULL_HANDLE;
    VkBuffer indexBuffer    = VK_NULL_HANDLE;
    VkBuffer clusterBuffer  = VK_NULL_HANDLE;
    VmaAllocation posAlloc  = VK_NULL_HANDLE;
    VmaAllocation idxAlloc  = VK_NULL_HANDLE;
    VmaAllocation cluAlloc  = VK_NULL_HANDLE;
};

struct NaniteGpuState {
    NaniteGpuBuffers buffers;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkPipelineLayout     pipeLayout = VK_NULL_HANDLE;
    VkPipeline           pipeline   = VK_NULL_HANDLE;
    VkDescriptorPool     descPool   = VK_NULL_HANDLE;
    VkDescriptorSet      descSet    = VK_NULL_HANDLE;
    uint32_t             clusterCount = 0;
};

struct ClusterGpu {
    uint32_t vertexOffset;
    uint32_t indexOffset;
    uint32_t triangleCount;
};

static_assert(sizeof(ClusterGpu) == 12);

bool UploadNaniteClusters(VkDevice device, VmaAllocator allocator,
                          Tumbler::CommandManager& cmdManager,
                          Tumbler::DeletionQueue& dq,
                          const std::vector<Tumbler::Nanite::Cluster>& clusters,
                          VkFormat colorFormat,
                          NaniteGpuState& state) {
    state.clusterCount = static_cast<uint32_t>(clusters.size());
    if (state.clusterCount == 0) return false;

    // ---- Build packed CPU-side buffers ----
    std::vector<glm::vec3> positions;          // all cluster positions
    std::vector<uint32_t>  allIndices;          // all cluster indices (ByteAddressBuffer)
    std::vector<ClusterGpu> clusterInfos;       // per-cluster metadata

    for (const auto& c : clusters) {
        ClusterGpu info{};
        info.vertexOffset   = static_cast<uint32_t>(positions.size());
        info.indexOffset    = static_cast<uint32_t>(allIndices.size());
        info.triangleCount  = 128;  // padded

        auto* verts = reinterpret_cast<const Tumbler::Vertex*>(c.VertexData.data());
        for (uint32_t v = 0; v < c.NumVertices; ++v) {
            positions.push_back(verts[v].pos);
        }
        for (auto idx : c.indices) {
            allIndices.push_back(idx);
        }
        clusterInfos.push_back(info);
    }

    // ---- Upload to GPU via staging ----
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

        // Staging upload
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

    UploadBuffer(positions.data(),    positions.size()    * sizeof(glm::vec3),
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, state.buffers.positionBuffer, state.buffers.posAlloc);
    UploadBuffer(allIndices.data(),   allIndices.size()   * sizeof(uint32_t),
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, state.buffers.indexBuffer, state.buffers.idxAlloc);
    UploadBuffer(clusterInfos.data(), clusterInfos.size() * sizeof(ClusterGpu),
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, state.buffers.clusterBuffer, state.buffers.cluAlloc);

    // ---- Descriptor set layout (Set 1: 3 SSBOs) ----
    VkDescriptorSetLayoutBinding bindings[3] = {
        { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
        { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
        { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
    };

    VkDescriptorSetLayoutCreateInfo setLayoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 3,
        .pBindings    = bindings,
    };
    vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &state.setLayout);

    // ---- Pipeline layout (Set 1 + push constant MVP) ----
    VkPushConstantRange pushRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset     = 0,
        .size       = sizeof(glm::mat4),
    };

    VkPipelineLayoutCreateInfo pipeLayoutInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &state.setLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &pushRange,
    };
    vkCreatePipelineLayout(device, &pipeLayoutInfo, nullptr, &state.pipeLayout);

    // ---- Descriptor pool + set ----
    VkDescriptorPoolSize poolSize{
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 3,
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

    // Write descriptors
    VkDescriptorBufferInfo posInfo{
        .buffer = state.buffers.positionBuffer,
        .range  = VK_WHOLE_SIZE,
    };
    VkDescriptorBufferInfo idxInfo{
        .buffer = state.buffers.indexBuffer,
        .range  = VK_WHOLE_SIZE,
    };
    VkDescriptorBufferInfo cluInfo{
        .buffer = state.buffers.clusterBuffer,
        .range  = VK_WHOLE_SIZE,
    };

    VkWriteDescriptorSet writes[3] = {
        { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, state.descSet, 0, 0, 1,
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &posInfo, nullptr },
        { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, state.descSet, 1, 0, 1,
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &idxInfo, nullptr },
        { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, state.descSet, 2, 0, 1,
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &cluInfo, nullptr },
    };
    vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);

    // ---- Graphics pipeline ----
    std::vector<VkFormat> formats = { colorFormat };
    std::span<VkFormat> colorSpan(formats);
    Tumbler::GraphicsPipelineBuilder pipeBuilder{
        .layout      = state.pipeLayout,
        .vertPath    = SHADER_DIR "/nanite_debug_vert.spv",
        .fragPath    = SHADER_DIR "/nanite_debug_frag.spv",
        .colorFormats = colorSpan,
        .depthFormat = VK_FORMAT_D32_SFLOAT,
        .cullMode    = VK_CULL_MODE_NONE,
    };

    auto pipe = pipeBuilder.Build(device);
    if (!pipe) return false;
    state.pipeline = *pipe;

    LOG_INFO("Nanite GPU upload: {} clusters, {} vertices, {} indices",
             state.clusterCount, positions.size(), allIndices.size());
    return true;
}

void DestroyNaniteGpuState(VkDevice device, VmaAllocator allocator,
                           Tumbler::DeletionQueue& dq,
                           NaniteGpuState& state) {
    if (state.pipeline)   dq.Enqueue([device, p = state.pipeline]() { vkDestroyPipeline(device, p, nullptr); });
    if (state.descPool)   dq.Enqueue([device, p = state.descPool]()  { vkDestroyDescriptorPool(device, p, nullptr); });
    if (state.setLayout)  dq.Enqueue([device, l = state.setLayout]() { vkDestroyDescriptorSetLayout(device, l, nullptr); });
    if (state.pipeLayout) dq.Enqueue([device, l = state.pipeLayout](){ vkDestroyPipelineLayout(device, l, nullptr); });
    if (state.buffers.positionBuffer) dq.Enqueue([allocator, b = state.buffers.positionBuffer, a = state.buffers.posAlloc]()
        { vmaDestroyBuffer(allocator, b, a); });
    if (state.buffers.indexBuffer) dq.Enqueue([allocator, b = state.buffers.indexBuffer, a = state.buffers.idxAlloc]()
        { vmaDestroyBuffer(allocator, b, a); });
    if (state.buffers.clusterBuffer) dq.Enqueue([allocator, b = state.buffers.clusterBuffer, a = state.buffers.cluAlloc]()
        { vmaDestroyBuffer(allocator, b, a); });
}

} // anonymous namespace

int main(int argc, char** argv) {
    using namespace Tumbler;

    Log::Init();

    // ---- Nanite: load mesh + partition ----
    const char* meshPath = (argc > 1) ? argv[1] : ASSET_DIR "/models/Sting-Sword-lowpoly.obj";
    auto mesh = LoadObj(meshPath);
    if (!mesh) {
        LOG_ERROR("Failed to load mesh: {}", meshPath);
        return 1;
    }
    LOG_INFO("Mesh loaded: {} vertices, {} triangles",
             mesh->vertices.size(), mesh->indices.size() / 3);

    auto naniteData = Nanite::NaniteBuilder().Build(*mesh);
    if (!naniteData) {
        LOG_ERROR("Nanite build failed");
        return 1;
    }
    auto& clusters = naniteData->clusterDag.GetClusters();
    LOG_INFO("Nanite partition: {} clusters", clusters.size());

    // ---- Window ----
    AppWindow window;
    if (!window.Init(1920, 1080, "Tumbler — Nanite Debug")) {
        LOG_ERROR("Window init failed");
        return 1;
    }

    // ---- Vulkan ----
    VulkanDevice device;
    if (!device.CreateInstance()) return 1;

    auto surface = window.CreateSurface(device.GetInstance());
    if (!surface) return 1;

    if (!device.CompleteInit(*surface)) return 1;

    // ---- VMA allocator ----
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

    // ---- Upload nanite clusters to GPU ----
    NaniteGpuState naniteGpu;
    if (!UploadNaniteClusters(device.GetDevice(), vmaAllocator,
                              cmdManager, deletionQueue, clusters,
                              swapchain.GetFormat(), naniteGpu)) {
        LOG_ERROR("Nanite GPU upload failed");
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
            if (fbW > 0 && fbH > 0) swapchain.Recreate(fbW, fbH);
            continue;
        }

        deletionQueue.Flush();

        uint32_t imageIndex = 0;
        VkResult acquireResult = swapchain.AcquireNextImage(&imageIndex, VK_NULL_HANDLE, acquireFence);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR) continue;
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) break;

        vkWaitForFences(device.GetDevice(), 1, &acquireFence, VK_TRUE, UINT64_MAX);
        vkResetFences(device.GetDevice(), 1, &acquireFence);

        // ---- Build view-proj matrix ----
        float aspect = static_cast<float>(swapchain.GetExtent().width) /
                       static_cast<float>(swapchain.GetExtent().height);
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.5f, 500.0f);
        proj[1][1] *= -1;  // Vulkan inverted Y
        // Model spans Z=-30..+30, X=-6..+6, Y=-0.8..+0.8
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 10.0f, 80.0f),
                                     glm::vec3(0.0f, 0.0f, 0.0f),
                                     glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 viewProj = proj * view;

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
            .clearValue  = { .color = {{ 0.2f, 0.1f, 0.3f, 1.0f }} },
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

        // ---- Draw nanite clusters ----
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, naniteGpu.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                naniteGpu.pipeLayout, 0, 1,
                                &naniteGpu.descSet, 0, nullptr);
        vkCmdPushConstants(cmd, naniteGpu.pipeLayout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(glm::mat4), &viewProj);

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

        // DEBUG: just draw 3 vertices (1 triangle), 1 instance
        vkCmdDraw(cmd, 3, 1, 0, 0);

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
    vkDeviceWaitIdle(device.GetDevice());

    vkDestroyFence(device.GetDevice(), acquireFence, nullptr);
    DestroyNaniteGpuState(device.GetDevice(), vmaAllocator, deletionQueue, naniteGpu);
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
