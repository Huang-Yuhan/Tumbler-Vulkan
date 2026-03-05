#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <string>

class VulkanRenderer;
class FMaterialInstance;

class FMaterial : public std::enable_shared_from_this<FMaterial> {
public:
    explicit FMaterial(VulkanRenderer* renderer);
    ~FMaterial();

    // 禁止拷贝
    FMaterial(const FMaterial&) = delete;
    FMaterial& operator=(const FMaterial&) = delete;

    // ==========================================
    // 核心 Vulkan 资源
    // ==========================================
    VkPipeline Pipeline = VK_NULL_HANDLE;
    VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout DescriptorSetLayout = VK_NULL_HANDLE;

    // ==========================================
    // 核心接口
    // ==========================================
    void BuildPipeline(const std::string& vertPath, const std::string& fragPath);
    std::shared_ptr<FMaterialInstance> CreateInstance();

private:
    VulkanRenderer* Renderer;
};