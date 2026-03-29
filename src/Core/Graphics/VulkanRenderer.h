#pragma once

#include "VulkanContext.h"
#include "VulkanSwapchain.h"
#include "RenderDevice.h"
#include "CommandBufferManager.h"
#include "ResourceUploadManager.h"
#include "FVulkanMesh.h"
#include "VulkanTypes.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

#include "SceneViewData.h"
#include "Core/Graphics/RenderPacket.h"
#include "Core/Graphics/IRenderPipeline.h"

class FTexture;
class AppWindow;
class FMesh;
class FAssetManager;

/**
 * @brief Vulkan 渲染器
 * @details 重构后的渲染器，职责更加单一：
 *          - 管理渲染流程和帧循环
 *          - 协调各个子系统
 *          - 不再直接处理 Buffer/Image 创建、命令缓冲管理等底层操作
 */
class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    // 禁止拷贝
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    void Init(AppWindow* window);
    void Cleanup();

    // ==========================================
    // 核心渲染接口
    // ==========================================

    /**
     * @brief 渲染一帧
     * @param viewData 场景视图数据
     * @param renderPackets 渲染包列表
     * @param onUIRender UI 渲染回调
     */
    void Render(
        const SceneViewData& viewData,
        const std::vector<RenderPacket>& renderPackets,
        std::function<void(VkCommandBuffer)> onUIRender = nullptr
    );

    // ==========================================
    // 资源上传接口（委托给 ResourceUploadManager）
    // ==========================================

    /**
     * @brief 上传 Mesh 到 GPU
     * @param cpuMesh CPU 端网格
     * @return GPU 端网格引用
     */
    FVulkanMesh& UploadMesh(FMesh* cpuMesh);

    /**
     * @brief 加载纹理
     * @param filePath 纹理文件路径
     * @return 加载的纹理
     */
    std::shared_ptr<FTexture> LoadTexture(const std::string& filePath);

    // ==========================================
    // Getter - 供其他系统使用
    // ==========================================

    [[nodiscard]] VkDevice GetDevice() const { return Context.GetDevice(); }
    [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const { return Context.GetPhysicalDevice(); }
    [[nodiscard]] VkQueue GetGraphicsQueue() const { return Context.GetGraphicsQueue(); }
    [[nodiscard]] VkDescriptorSetLayout GetGlobalSetLayout() const { return GlobalSetLayout; }
    [[nodiscard]] const VulkanContext& GetContext() const { return Context; }
    [[nodiscard]] RenderDevice* GetRenderDevice() { return &TheRenderDevice; }
    [[nodiscard]] CommandBufferManager* GetCommandBufferManager() { return &TheCommandBufferManager; }
    [[nodiscard]] ResourceUploadManager* GetResourceUploadManager() { return &TheResourceUploadManager; }
    [[nodiscard]] FAssetManager* GetAssetManager() const { return AssetManager; }

    // ==========================================
    // 供 FMaterial / 渲染管线 使用的接口
    // ==========================================

    [[nodiscard]] VkFormat GetSwapchainImageFormat() const { return SwapChain.GetImageFormat(); }
    [[nodiscard]] VkFormat GetSwapchainDepthFormat() const { return SwapChain.GetDepthFormat(); }
    [[nodiscard]] const std::vector<VkImageView>& GetSwapchainImageViews() const { return SwapChain.GetImageViews(); }
    [[nodiscard]] VkImageView GetSwapchainDepthImageView() const { return SwapChain.GetDepthImageView(); }

    [[nodiscard]] VkRenderPass GetRenderPass(ERenderPath path = ERenderPath::Forward) const;
    [[nodiscard]] VkExtent2D GetSwapchainExtent() const { return SwapChain.GetExtent(); }
    [[nodiscard]] uint32_t GetSwapchainImageCount() const { return static_cast<uint32_t>(SwapChain.GetImageCount()); }
    [[nodiscard]] VkDescriptorPool GetDescriptorPool() const { return DescriptorPool; }
    [[nodiscard]] VkDescriptorSet GetGlobalDescriptorSet() const { return GlobalDescriptorSet; }

    /**
     * @brief 分配描述符集
     * @param layout 描述符集布局
     * @return 分配的描述符集
     */
    VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout);

    // ==========================================
    // 向后兼容的委托方法
    // ==========================================

    void CreateBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, AllocatedBuffer& outBuffer);
    void DestroyBuffer(AllocatedBuffer& buffer);
    bool LoadShaderModule(const char* filePath, VkShaderModule* outShaderModule);

private:
    AppWindow* Window = nullptr;
    FAssetManager* AssetManager = nullptr;

    // ==========================================
    // 子系统
    // ==========================================

    VulkanContext Context;
    VulkanSwapchain SwapChain;
    class RenderDevice TheRenderDevice;
    class CommandBufferManager TheCommandBufferManager;
    class ResourceUploadManager TheResourceUploadManager;

    // ==========================================
    // 渲染管线策略 (Pipelines)
    // ==========================================

    std::unordered_map<ERenderPath, std::unique_ptr<IRenderPipeline>> Pipelines;

    // ==========================================
    // 同步对象
    // ==========================================

    VkSemaphore ImageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore RenderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence RenderFence = VK_NULL_HANDLE;

    // ==========================================
    // 描述符管理
    // ==========================================

    VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout GlobalSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet GlobalDescriptorSet = VK_NULL_HANDLE;
    AllocatedBuffer SceneParameterBuffer{};

    // ==========================================
    // 命令缓冲区（每帧重用）
    // ==========================================

    VkCommandBuffer MainCommandBuffer = VK_NULL_HANDLE;

    // ==========================================
    // 内部初始化方法
    // ==========================================

    void InitPipelines();
    void InitSyncStructures();
    void InitDescriptors();

    // ==========================================
    // 渲染方法
    // ==========================================

    void RecordCommandBuffer(
        VkCommandBuffer cmd,
        uint32_t imageIndex,
        const SceneViewData& viewData,
        const std::vector<RenderPacket>& renderPackets,
        std::function<void(VkCommandBuffer)> onUIRender
    );

    bool RecreateSwapchain();
};
