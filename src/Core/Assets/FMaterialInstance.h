#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include <Core/Math/Math.h>
#include "Core/Graphics/VulkanTypes.h"

class FMaterial;
class FTexture;
class VulkanRenderer;

// GPU UBO 结构 — 保持 glm 以保证 std140 对齐
struct FMaterialUBO {
    glm::vec4 BaseColorTint = glm::vec4(1.0f);
    float Roughness = 0.5f;
    float Metallic = 0.0f;
    float NormalMapStrength = 1.0f;
    int32_t TwoSided = 0;
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
    void SetVector(const std::string& name, const Tumbler::Math::Vector4f& value);
    void SetFloat(const std::string& name, float value);
    void SetTwoSided(bool twoSided);

    // ==========================================
    // 核心流转：将 CPU 参数提交给 GPU
    // ==========================================
    void ApplyChanges();
    
    // 快速更新 UBO 参数（不重新绑定描述符）
    void UpdateUBO();

    [[nodiscard]] VkDescriptorSet GetDescriptorSet() const { return DescriptorSet; }
    [[nodiscard]] std::shared_ptr<FMaterial> GetParent() const { return ParentMaterial; }

    // ==========================================
    // 材质参数 Getter
    // ==========================================
    [[nodiscard]] const FMaterialUBO& GetParameters() const { return ParameterData; }
    [[nodiscard]] Tumbler::Math::Vector4f GetBaseColorTint() const {
        return Tumbler::Math::Vector4f{ParameterData.BaseColorTint.x, ParameterData.BaseColorTint.y,
                                       ParameterData.BaseColorTint.z, ParameterData.BaseColorTint.w};
    }
    [[nodiscard]] float GetRoughness() const { return ParameterData.Roughness; }
    [[nodiscard]] float GetMetallic() const { return ParameterData.Metallic; }
    [[nodiscard]] float GetNormalMapStrength() const { return ParameterData.NormalMapStrength; }
    [[nodiscard]] bool IsTwoSided() const { return ParameterData.TwoSided != 0; }

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