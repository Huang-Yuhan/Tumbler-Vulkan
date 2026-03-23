#include "FMaterialInstance.h"
#include "FMaterial.h"
#include "Core/Assets/FTexture.h"
#include "Core/Assets/FAssetManager.h"
#include "Core/Graphics/VulkanRenderer.h"
#include "Core/Utils/Log.h"
#include <cstring>
#include <vector>

FMaterialInstance::FMaterialInstance(std::shared_ptr<FMaterial> parentMaterial, VulkanRenderer* renderer, FAssetManager* assetMgr, VkDescriptorSet descriptorSet)
    : ParentMaterial(std::move(parentMaterial)), Renderer(renderer), AssetManager(assetMgr), DescriptorSet(descriptorSet) 
{
    // 【核心】实例创建时，向 VMA 申请一块“CPU 可写，GPU 可读”的显存存放 UBO
    Renderer->CreateBuffer(
        sizeof(FMaterialUBO), 
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
        VMA_MEMORY_USAGE_AUTO_PREFER_HOST, // 自动保持 Map 状态，方便 CPU 随时修改
        UBOBuffer
    );
}

FMaterialInstance::~FMaterialInstance() {
    if (Renderer && UBOBuffer.Buffer != VK_NULL_HANDLE) {
        Renderer->DestroyBuffer(UBOBuffer);
    }
}

void FMaterialInstance::SetTexture(const std::string& name, std::shared_ptr<FTexture> texture) {
    if (texture) Textures[name] = std::move(texture);
}

void FMaterialInstance::SetVector(const std::string& name, const glm::vec4& value) {
    if (name == "BaseColorTint") ParameterData.BaseColorTint = value;
}

void FMaterialInstance::SetFloat(const std::string& name, float value) {
    if (name == "Roughness") ParameterData.Roughness = value;
    else if (name == "Metallic") ParameterData.Metallic = value;
    else if (name == "NormalMapStrength") ParameterData.NormalMapStrength = value;
}

void FMaterialInstance::SetTwoSided(bool twoSided) {
    ParameterData.TwoSided = twoSided ? 1 : 0;
}

void FMaterialInstance::ApplyChanges() {
    // 1. 将 CPU 中的 UBO 数据拷贝到显存 (因为是 AUTO_PREFER_HOST，所以 pMappedData 一直可用)
    memcpy(UBOBuffer.Info.pMappedData, &ParameterData, sizeof(FMaterialUBO));

    // 2. 准备更新 Vulkan 描述符集
    std::vector<VkWriteDescriptorSet> descriptorWrites;

    // --- (A) 配置 BaseColorMap (Binding 0) ---
    std::shared_ptr<FTexture> baseColorTex = nullptr;
    if (Textures.find("BaseColorMap") != Textures.end()) {
        baseColorTex = Textures["BaseColorMap"];
    } else {
        baseColorTex = AssetManager->GetOrLoadTexture("DefaultWhite", "assets/textures/white.png");
    }

    if (baseColorTex) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = baseColorTex->GetImageView();
        imageInfo.sampler = baseColorTex->GetSampler();

        VkWriteDescriptorSet texWrite{};
        texWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        texWrite.dstSet = DescriptorSet;
        texWrite.dstBinding = 0;
        texWrite.dstArrayElement = 0;
        texWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        texWrite.descriptorCount = 1;
        texWrite.pImageInfo = &imageInfo;
        descriptorWrites.push_back(texWrite);
    }

    // --- (B) 配置 NormalMap (Binding 1) ---
    std::shared_ptr<FTexture> normalMapTex = nullptr;
    if (Textures.find("NormalMap") != Textures.end()) {
        normalMapTex = Textures["NormalMap"];
    } else {
        normalMapTex = AssetManager->GetOrLoadTexture("DefaultWhite", "assets/textures/white.png");
    }

    if (normalMapTex) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = normalMapTex->GetImageView();
        imageInfo.sampler = normalMapTex->GetSampler();

        VkWriteDescriptorSet texWrite{};
        texWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        texWrite.dstSet = DescriptorSet;
        texWrite.dstBinding = 1;
        texWrite.dstArrayElement = 0;
        texWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        texWrite.descriptorCount = 1;
        texWrite.pImageInfo = &imageInfo;
        descriptorWrites.push_back(texWrite);
    }

    // --- (C) 配置 UBO (Binding 2) ---
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = UBOBuffer.Buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(FMaterialUBO);

    VkWriteDescriptorSet uboWrite{};
    uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    uboWrite.dstSet = DescriptorSet;
    uboWrite.dstBinding = 2; 
    uboWrite.dstArrayElement = 0;
    uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboWrite.descriptorCount = 1;
    uboWrite.pBufferInfo = &bufferInfo;
    descriptorWrites.push_back(uboWrite);

    // 3. 提交给显卡
    if (!descriptorWrites.empty()) {
        vkUpdateDescriptorSets(Renderer->GetDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}