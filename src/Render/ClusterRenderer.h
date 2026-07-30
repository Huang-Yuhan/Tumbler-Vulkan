#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <glm/glm.hpp>

#include <vector>

namespace Tumbler {

class CommandManager;
class DeletionQueue;
namespace Nanite { struct Cluster; }

// ────────────────────────────────────────────────────────────
// ClusterRenderer — owns GPU resources for cluster rendering.
// Uploads cluster data once, renders into arbitrary render
// targets with a given view-projection matrix.
// ────────────────────────────────────────────────────────────
class ClusterRenderer {
public:
    enum class Mode { Shaded, Wireframe, ShadedWireframe };

    ClusterRenderer() = default;
    ~ClusterRenderer() = default;

    ClusterRenderer(const ClusterRenderer&)            = delete;
    ClusterRenderer& operator=(const ClusterRenderer&) = delete;

    // Upload cluster data to GPU, create pipelines.
    // Returns false on failure.
    bool Init(VkDevice device, VmaAllocator allocator,
              CommandManager& cmdManager,
              const std::vector<Nanite::Cluster>& clusters,
              VkFormat colorFormat);

    // Enqueue all GPU resources for deferred destruction.
    void Shutdown(DeletionQueue& dq);

    // Record draw commands into cmd. Caller manages render pass / layout.
    void Render(VkCommandBuffer cmd, const glm::mat4& viewProj,
                Mode mode, VkExtent2D extent) const;

    uint32_t GetVertexCount()  const { return m_VertexCount; }
    uint32_t GetNumClusters()  const { return m_NumClusters; }

private:
    void CreatePipelines(VkDevice device, VkFormat colorFormat);

    VkDevice      m_Device      = VK_NULL_HANDLE;
    VmaAllocator  m_Allocator   = VK_NULL_HANDLE;

    VkBuffer           m_ClusterMeta    = VK_NULL_HANDLE;
    VkBuffer           m_IndexBuffer    = VK_NULL_HANDLE;
    VkBuffer           m_PositionBuffer = VK_NULL_HANDLE;
    VmaAllocation      m_MetaAlloc      = VK_NULL_HANDLE;
    VmaAllocation      m_IdxAlloc       = VK_NULL_HANDLE;
    VmaAllocation      m_PosAlloc       = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_SetLayout   = VK_NULL_HANDLE;
    VkDescriptorPool      m_DescPool    = VK_NULL_HANDLE;
    VkDescriptorSet       m_DescSet     = VK_NULL_HANDLE;
    VkPipelineLayout      m_PipeLayout  = VK_NULL_HANDLE;
    VkPipeline            m_PipelineFill  = VK_NULL_HANDLE;
    VkPipeline            m_PipelineLine  = VK_NULL_HANDLE;
    uint32_t              m_VertexCount   = 0;
    uint32_t              m_NumClusters   = 0;
};

} // namespace Tumbler
