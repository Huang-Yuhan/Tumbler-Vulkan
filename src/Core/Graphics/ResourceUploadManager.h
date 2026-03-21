#pragma once

#include <vulkan/vulkan.h>
#include <unordered_map>
#include <memory>
#include <string>

#include "VulkanTypes.h"

// 前置声明
class VulkanContext;
class RenderDevice;
class CommandBufferManager;
class FMesh;
class FVulkanMesh;
class FTexture;

/**
 * @brief 资源上传管理器
 * @details 负责 Mesh 和 Texture 的异步/同步上传管理
 * @note 这是 VulkanRenderer 拆分后的子系统，职责单一
 */
class ResourceUploadManager {
public:
    ResourceUploadManager();
    ~ResourceUploadManager();

    // 禁止拷贝
    ResourceUploadManager(const ResourceUploadManager&) = delete;
    ResourceUploadManager& operator=(const ResourceUploadManager&) = delete;

    // 允许移动
    ResourceUploadManager(ResourceUploadManager&& other) noexcept;
    ResourceUploadManager& operator=(ResourceUploadManager&& other) noexcept;

    /**
     * @brief 初始化资源上传管理器
     * @param renderDevice 渲染设备
     * @param commandBufferManager 命令缓冲区管理器
     */
    void Init(RenderDevice* renderDevice, CommandBufferManager* commandBufferManager);

    /**
     * @brief 清理所有资源
     */
    void Cleanup();

    // ==========================================
    // Mesh 上传
    // ==========================================

    /**
     * @brief 上传 Mesh 到 GPU
     * @param cpuMesh CPU 端网格数据
     * @return GPU 端网格引用
     */
    FVulkanMesh& UploadMesh(FMesh* cpuMesh);

    /**
     * @brief 检查 Mesh 是否已上传
     * @param cpuMesh CPU 端网格
     * @return 是否已上传
     */
    [[nodiscard]] bool IsMeshUploaded(FMesh* cpuMesh) const;

    /**
     * @brief 获取已上传的 Mesh
     * @param cpuMesh CPU 端网格
     * @return GPU 端网格引用
     */
    [[nodiscard]] FVulkanMesh& GetUploadedMesh(FMesh* cpuMesh);

    /**
     * @brief 清理所有 Mesh 缓存
     */
    void ClearMeshCache();

    // ==========================================
    // Texture 加载
    // ==========================================

    /**
     * @brief 从文件加载纹理
     * @param filePath 纹理文件路径
     * @return 加载的纹理对象
     */
    std::shared_ptr<FTexture> LoadTexture(const std::string& filePath);

    /**
     * @brief 从内存创建纹理
     * @param pixels 像素数据
     * @param width 图像宽度
     * @param height 图像高度
     * @param channels 通道数
     * @return 创建的纹理对象
     */
    std::shared_ptr<FTexture> CreateTextureFromMemory(
        const unsigned char* pixels,
        int width,
        int height,
        int channels
    );

    // ==========================================
    // Shader 加载
    // ==========================================

    /**
     * @brief 加载 Shader 模块
     * @param filePath Shader 文件路径
     * @param outShaderModule 输出 Shader 模块
     * @return 是否加载成功
     */
    bool LoadShaderModule(const char* filePath, VkShaderModule* outShaderModule);

    // ==========================================
    // Getter
    // ==========================================

    [[nodiscard]] RenderDevice* GetRenderDevice() const { return RenderDeviceRef; }
    [[nodiscard]] CommandBufferManager* GetCommandBufferManager() const { return CommandBufferManagerRef; }

private:
    RenderDevice* RenderDeviceRef = nullptr;
    CommandBufferManager* CommandBufferManagerRef = nullptr;
    VkDevice Device = VK_NULL_HANDLE;

    // Mesh 缓存
    std::unordered_map<FMesh*, FVulkanMesh> MeshCache;
};
