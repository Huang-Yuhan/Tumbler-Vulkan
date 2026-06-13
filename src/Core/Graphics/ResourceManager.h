// ResourceManager.h — 统一资源管理 (Mesh/纹理/Shader)
//
// 职责: Unified VB/IB sub-allocation, Mesh 上传 (tinyobjloader),
//       纹理上传 (stb_image + Mipmap), Shader Module 加载,
//       建纹理索引表为 Bindless 做准备.
//
// 依赖: GpuDevice, CommandManager
// 层级: 图形基础设施 (Phase 3)

#pragma once

#include <glm/glm.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Tumbler {

class GpuDevice;
class CommandManager;

struct MeshHandle {
    uint32_t VertexOffset = 0;
    uint32_t IndexOffset = 0;
    uint32_t VertexCount = 0;
    uint32_t IndexCount = 0;
    glm::vec4 BoundingSphere{0.0f, 0.0f, 0.0f, 1.0f};
};

struct TextureHandle {
    VkImage Image = VK_NULL_HANDLE;
    VkImageView ImageView = VK_NULL_HANDLE;
    uint32_t TextureIndex = 0;
};

class ResourceManager {
public:
    bool Init(VkDevice device, GpuDevice& renderDevice, CommandManager& commandManager);
    void Shutdown();

    // Mesh
    MeshHandle UploadMesh(const std::string& objPath);
    void DestroyMesh(const MeshHandle& handle);

    // Texture
    TextureHandle UploadTexture(const std::string& path);
    void DestroyTexture(const TextureHandle& handle);

    // Shader
    VkShaderModule LoadShader(const std::string& path);
    void DestroyShaderModule(VkShaderModule module);

    // Texture index lookup (for Bindless Set 1)
    uint32_t GetTextureCount() const { return static_cast<uint32_t>(m_TextureViews.size()); }
    const std::vector<VkImageView>& GetTextureViews() const { return m_TextureViews; }

    ResourceManager() = default;
    ~ResourceManager() = default;
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    GpuDevice* m_GpuDevice = nullptr;
    CommandManager* m_CommandManager = nullptr;

    // Unified buffers
    VkBuffer m_VertexBuffer = VK_NULL_HANDLE;
    VmaAllocation m_VertexBufferAlloc = VK_NULL_HANDLE;
    VkBuffer m_IndexBuffer = VK_NULL_HANDLE;
    VmaAllocation m_IndexBufferAlloc = VK_NULL_HANDLE;
    uint32_t m_VertexBufferOffset = 0;
    uint32_t m_IndexBufferOffset = 0;

    // Texture index map
    std::unordered_map<std::string, uint32_t> m_TextureIndexMap;
    std::vector<VkImageView> m_TextureViews;
    std::vector<VkImage> m_TextureImages;
    std::vector<VmaAllocation> m_TextureAllocations;
};

} // namespace Tumbler
