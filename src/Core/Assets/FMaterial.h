#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <string>

class RenderDevice;
class VulkanRenderer;
class FMaterialInstance;
class FAssetManager;

class FMaterial : public std::enable_shared_from_this<FMaterial> {
public:
    FMaterial(
        RenderDevice* renderDevice,
        VkRenderPass renderPass,
        VkDescriptorSetLayout globalSetLayout,
        VkExtent2D swapchainExtent,
        FAssetManager* assetMgr,
        VulkanRenderer* renderer
    );

    ~FMaterial();

    // 禁止拷贝
    FMaterial(const FMaterial&) = delete;
    FMaterial& operator=(const FMaterial&) = delete;

    // 允许移动
    FMaterial(FMaterial&& other) noexcept;
    FMaterial& operator=(FMaterial&& other) noexcept;

    // ==========================================
    // 核心 Vulkan 资源
    // ==========================================
    VkPipeline Pipeline = VK_NULL_HANDLE;
    VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout DescriptorSetLayout = VK_NULL_HANDLE;

    // ==========================================
    // 核心接口
    // ==========================================

    /**
     * @brief 构建渲染管线
     * @param vertPath 顶点着色器路径
     * @param fragPath 片段着色器路径
     */
    void BuildPipeline(const std::string& vertPath, const std::string& fragPath);

    /**
     * @brief 创建材质实例
     * @return 新创建的材质实例
     */
    std::shared_ptr<FMaterialInstance> CreateInstance();

    // ==========================================
    // Getter
    // ==========================================

    [[nodiscard]] RenderDevice* GetRenderDevice() const { return RenderDeviceRef; }
    [[nodiscard]] VkRenderPass GetRenderPass() const { return RenderPass; }
    [[nodiscard]] VkDescriptorSetLayout GetGlobalSetLayout() const { return GlobalSetLayout; }
    [[nodiscard]] VkExtent2D GetSwapchainExtent() const { return SwapchainExtent; }
    [[nodiscard]] FAssetManager* GetAssetManager() const { return AssetManager; }

private:
    RenderDevice* RenderDeviceRef = nullptr;
    VkRenderPass RenderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout GlobalSetLayout = VK_NULL_HANDLE;
    VkExtent2D SwapchainExtent{};
    FAssetManager* AssetManager = nullptr;
    VulkanRenderer* Renderer = nullptr;
};
