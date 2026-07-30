#include "ClusterRenderer.h"
#include "Assets/MeshLoader.h"
#include "Core/Utils/Log.h"
#include "Gfx/CommandManager.h"
#include "Gfx/DeletionQueue.h"
#include "Gfx/PipelineBuilder.h"
#include "Render/Nanite/Cluster.h"
#include "Render/Nanite/NaniteDefinition.h"

#include <glm/glm.hpp>

#include <cmath>
#include <cstring>
#include <span>

namespace Tumbler {

using namespace Nanite;

namespace {

struct PushData { glm::mat4 viewProj; uint32_t wireframe; };
static_assert(sizeof(PushData) >= sizeof(glm::mat4) + sizeof(uint32_t));

struct CpuClusterInfo {
    uint32_t indexOffset, vertexOffset, indexCount, pad;
    float color[4];
};

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
        if      (hue < 1.0f / 6.0f) { r = c; g = x; b = 0; }
        else if (hue < 2.0f / 6.0f) { r = x; g = c; b = 0; }
        else if (hue < 3.0f / 6.0f) { r = 0; g = c; b = x; }
        else if (hue < 4.0f / 6.0f) { r = 0; g = x; b = c; }
        else if (hue < 5.0f / 6.0f) { r = x; g = 0; b = c; }
        else                         { r = c; g = 0; b = x; }
        colors[i] = glm::vec4(r + m, g + m, b + m, 1.0f);
    }
    return colors;
}

} // anonymous namespace

bool ClusterRenderer::Init(VkDevice device, VmaAllocator allocator,
                           CommandManager& cmdManager,
                           const std::vector<Nanite::Cluster>& clusters,
                           VkFormat colorFormat) {
    if (clusters.empty()) return false;

    m_Device      = device;
    m_Allocator   = allocator;
    m_NumClusters = static_cast<uint32_t>(clusters.size());
    m_VertexCount = kClusterTriangleCount * 3;

    // ── Build flat CPU data ──
    std::vector<float>    allPositions;
    std::vector<uint32_t> allIndices;
    std::vector<glm::vec4> clusterColors = BuildClusterColors(m_NumClusters);
    std::vector<CpuClusterInfo> meta(m_NumClusters);

    uint32_t curIdxOff  = 0;
    uint32_t curVertOff = 0;

    for (uint32_t c = 0; c < m_NumClusters; ++c) {
        const auto& cl = clusters[c];
        uint32_t nv = cl.NumVertices;
        uint32_t ni = static_cast<uint32_t>(cl.indices.size());

        for (uint32_t v = 0; v < nv; ++v) {
            const float* src = reinterpret_cast<const float*>(
                cl.VertexData.data() + v * sizeof(Vertex));
            allPositions.insert(allPositions.end(), { src[0], src[1], src[2] });
        }
        allIndices.insert(allIndices.end(), cl.indices.begin(), cl.indices.end());

        meta[c] = { curIdxOff, curVertOff, ni, 0,
                    { clusterColors[c].x, clusterColors[c].y,
                      clusterColors[c].z, clusterColors[c].w } };
        curIdxOff  += ni;
        curVertOff += nv;
    }

    // ── Upload to GPU ──
    auto Upload = [&](const void* data, VkDeviceSize size,
                      VkBufferUsageFlags usage,
                      VkBuffer& buf, VmaAllocation& alloc) {
        VkBufferCreateInfo bufInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = size,
            .usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        };
        VmaAllocationCreateInfo allocInfo{ .usage = VMA_MEMORY_USAGE_GPU_ONLY };
        vmaCreateBuffer(allocator, &bufInfo, &allocInfo, &buf, &alloc, nullptr);

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
            vkCmdCopyBuffer(cmd, staging, buf, 1, &region);
        });
        // Staging buffer freed after GPU copy completes
        vmaDestroyBuffer(allocator, staging, stagingAlloc);
    };

    Upload(meta.data(), meta.size() * sizeof(CpuClusterInfo),
           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_ClusterMeta, m_MetaAlloc);
    Upload(allIndices.data(), allIndices.size() * sizeof(uint32_t),
           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_IndexBuffer, m_IdxAlloc);
    Upload(allPositions.data(), allPositions.size() * sizeof(float),
           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_PositionBuffer, m_PosAlloc);

    // ── Descriptor set ──
    VkDescriptorSetLayoutBinding bindings[] = {
        { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT },
        { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT },
        { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT },
    };
    VkDescriptorSetLayoutCreateInfo setLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 3, .pBindings = bindings,
    };
    vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &m_SetLayout);

    VkDescriptorPoolSize poolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 };
    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &poolSize,
    };
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescPool);

    VkDescriptorSetAllocateInfo setAlloc{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_DescPool, .descriptorSetCount = 1,
        .pSetLayouts = &m_SetLayout,
    };
    vkAllocateDescriptorSets(device, &setAlloc, &m_DescSet);

    VkDescriptorBufferInfo bufInfos[] = {
        { m_ClusterMeta,    0, VK_WHOLE_SIZE },
        { m_IndexBuffer,    0, VK_WHOLE_SIZE },
        { m_PositionBuffer, 0, VK_WHOLE_SIZE },
    };
    VkWriteDescriptorSet writes[3];
    for (int i = 0; i < 3; ++i) {
        writes[i] = {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = m_DescSet,
            .dstBinding      = static_cast<uint32_t>(i),
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo     = &bufInfos[i],
        };
    }
    vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);

    // ── Pipeline layout ──
    VkPushConstantRange pushRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size       = sizeof(PushData),
    };
    VkPipelineLayoutCreateInfo pipeLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &m_SetLayout,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &pushRange,
    };
    vkCreatePipelineLayout(device, &pipeLayoutInfo, nullptr, &m_PipeLayout);

    // ── Pipelines ──
    CreatePipelines(device, colorFormat);

    LOG_INFO("ClusterRenderer: {} clusters uploaded", m_NumClusters);
    return true;
}

void ClusterRenderer::CreatePipelines(VkDevice device, VkFormat colorFormat) {
    std::vector<VkFormat> formats = { colorFormat };
    std::span<VkFormat> colorSpan(formats);
    std::span<VkVertexInputBindingDescription>   noBindings;
    std::span<VkVertexInputAttributeDescription> noAttribs;

    GraphicsPipelineBuilder pipeBuilder{
        .layout         = m_PipeLayout,
        .vertPath       = SHADER_DIR "/cluster_draw_vert.spv",
        .fragPath       = SHADER_DIR "/cluster_draw_frag.spv",
        .colorFormats   = colorSpan,
        .depthFormat    = VK_FORMAT_D32_SFLOAT,
        .cullMode       = VK_CULL_MODE_BACK_BIT,
        .vertexBindings = noBindings,
        .vertexAttribs  = noAttribs,
    };

    auto fillPipe = pipeBuilder.Build(device);
    if (fillPipe) m_PipelineFill = *fillPipe;

    pipeBuilder.polygonMode       = VK_POLYGON_MODE_LINE;
    pipeBuilder.depthBiasEnable   = VK_TRUE;
    pipeBuilder.depthBiasConstant = -1.0f;
    pipeBuilder.depthBiasSlope    = -1.0f;

    auto linePipe = pipeBuilder.Build(device);
    if (linePipe) m_PipelineLine = *linePipe;
}

void ClusterRenderer::Shutdown(DeletionQueue& dq) {
    if (m_PipelineFill) dq.Enqueue([d = m_Device, p = m_PipelineFill]() { vkDestroyPipeline(d, p, nullptr); });
    if (m_PipelineLine) dq.Enqueue([d = m_Device, p = m_PipelineLine]() { vkDestroyPipeline(d, p, nullptr); });
    if (m_PipeLayout)   dq.Enqueue([d = m_Device, l = m_PipeLayout]()  { vkDestroyPipelineLayout(d, l, nullptr); });
    if (m_DescPool)     dq.Enqueue([d = m_Device, p = m_DescPool]()    { vkDestroyDescriptorPool(d, p, nullptr); });
    if (m_SetLayout)    dq.Enqueue([d = m_Device, l = m_SetLayout]()   { vkDestroyDescriptorSetLayout(d, l, nullptr); });
    if (m_ClusterMeta)  dq.Enqueue([a = m_Allocator, b = m_ClusterMeta,  al = m_MetaAlloc]() { vmaDestroyBuffer(a, b, al); });
    if (m_IndexBuffer)  dq.Enqueue([a = m_Allocator, b = m_IndexBuffer, al = m_IdxAlloc]()  { vmaDestroyBuffer(a, b, al); });
    if (m_PositionBuffer) dq.Enqueue([a = m_Allocator, b = m_PositionBuffer, al = m_PosAlloc](){ vmaDestroyBuffer(a, b, al); });
}

void ClusterRenderer::Render(VkCommandBuffer cmd,
                              const glm::mat4& viewProj,
                              Mode mode,
                              VkExtent2D extent) const {
    auto DrawPass = [&](VkPipeline pipeline, bool wireframe) {
        PushData pushData{ viewProj, wireframe ? 1u : 0u };
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_PipeLayout, 0, 1, &m_DescSet, 0, nullptr);
        vkCmdPushConstants(cmd, m_PipeLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushData), &pushData);

        VkViewport viewport{
            0.0f, 0.0f,
            static_cast<float>(extent.width),
            static_cast<float>(extent.height),
            0.0f, 1.0f,
        };
        VkRect2D scissor{ { 0, 0 }, extent };
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdDraw(cmd, m_VertexCount, m_NumClusters, 0, 0);
    };

    switch (mode) {
    case Mode::Shaded:    DrawPass(m_PipelineFill, false); break;
    case Mode::Wireframe: DrawPass(m_PipelineLine, true);  break;
    case Mode::ShadedWireframe:
        DrawPass(m_PipelineFill, false);
        DrawPass(m_PipelineLine, true);
        break;
    }
}

} // namespace Tumbler
