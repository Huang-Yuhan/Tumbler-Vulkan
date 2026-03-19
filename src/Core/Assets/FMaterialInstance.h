#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include "Core/Graphics/VulkanTypes.h"

class FMaterial;
class FTexture;
class VulkanRenderer;

// 【关键】必须和 pbr.frag 中的 MaterialParams 保持绝对一致 (std140 对齐)
struct FMaterialUBO {
    glm::vec4 BaseColorTint = glm::vec4(1.0f); // 默认白色，不染色
    float Roughness = 0.5f;
    float Metallic = 0.0f;
    glm::vec2 Padding; // 凑齐 16 字节对齐
};

class FAssetManager;

class FMaterialInstance {
public:
    FMaterialInstance(std::shared_ptr<FMaterial> parentMaterial, VulkanRenderer* renderer, FAssetManager* assetMgr, VkDescriptorSet descriptorSet);
    ~FMaterialInstance(); // 必须自定义析构，用于释放 UBO 显存

    // ==========================================
    // 材质参数 API
    // ==========================================
    void SetTexture(const std::string& name, std::shared_ptr<FTexture> texture);
    void SetVector(const std::string& name, const glm::vec4& value);
    void SetFloat(const std::string& name, float value);

    // ==========================================
    // 核心流转：将 CPU 参数提交给 GPU
    // ==========================================
    void ApplyChanges();

    [[nodiscard]] VkDescriptorSet GetDescriptorSet() const { return DescriptorSet; }
    [[nodiscard]] std::shared_ptr<FMaterial> GetParent() const { return ParentMaterial; }

private:
    std::shared_ptr<FMaterial> ParentMaterial;
    VulkanRenderer* Renderer;
    FAssetManager* AssetManager;
    VkDescriptorSet DescriptorSet = VK_NULL_HANDLE;

    // 逻辑数据镜像
    std::unordered_map<std::string, std::shared_ptr<FTexture>> Textures;
    FMaterialUBO ParameterData;

    // 物理显存资源
    AllocatedBuffer UBOBuffer{}; // 存放参数的 GPU 显存缓冲
};