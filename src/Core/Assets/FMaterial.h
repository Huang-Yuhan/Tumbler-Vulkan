#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <unordered_map>
#include "Core/Graphics/SceneViewData.h"

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
    std::unordered_map<ERenderPath, VkPipeline> Pipelines;
    VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout DescriptorSetLayout = VK_NULL_HANDLE;

    // ==========================================
    // 核心接口
    // ==========================================

    /**
     * @brief 构建多重渲染管线 (前向与延迟)
     * @param forwardVert 顶点着色器 (共享通道)
     * @param forwardFrag 前向片元着色器
     * @param deferredFrag 延迟几何缓冲片元着色器
     */
    void BuildPipelines(const std::string& forwardVert, const std::string& forwardFrag, const std::string& deferredFrag);

    /**
     * @brief 创建材质实例
     * @return 新创建的材质实例
     */
    std::shared_ptr<FMaterialInstance> CreateInstance();

    // ==========================================
    // Getter
    // ==========================================
    
    [[nodiscard]] VkPipeline GetPipeline(ERenderPath path) const {
        auto it = Pipelines.find(path);
        return it != Pipelines.end() ? it->second : VK_NULL_HANDLE;
    }

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
