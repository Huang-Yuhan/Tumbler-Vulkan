#include "DebugTexturePreview.h"
#include "Core/Graphics/VulkanRenderer.h"

void DebugTexturePreview::Init(VulkanRenderer* renderer)
{
    if (renderer == nullptr) {
        return;
    }

    Renderer = renderer;
    VkDevice device = renderer->GetDevice();

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 1.0f;
    vkCreateSampler(device, &samplerInfo, nullptr, &Sampler);

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &SetLayout);

    for (int i = 0; i < kMaxSlots; ++i) {
        DescriptorSets[i] = renderer->AllocateDescriptorSet(SetLayout);
    }
}

void DebugTexturePreview::Cleanup(VkDevice device)
{
    if (Sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, Sampler, nullptr);
        Sampler = VK_NULL_HANDLE;
    }
    if (SetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, SetLayout, nullptr);
        SetLayout = VK_NULL_HANDLE;
    }
    for (int i = 0; i < kMaxSlots; ++i) {
        DescriptorSets[i] = VK_NULL_HANDLE;
    }
    Renderer = nullptr;
}

void DebugTexturePreview::SetImage(int slot, VkImageView imageView)
{
    if (Renderer == nullptr || Sampler == VK_NULL_HANDLE) {
        return;
    }
    if (slot < 0 || slot >= kMaxSlots || DescriptorSets[slot] == VK_NULL_HANDLE) {
        return;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = Sampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = DescriptorSets[slot];
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(Renderer->GetDevice(), 1, &write, 0, nullptr);
}

VkDescriptorSet DebugTexturePreview::GetTextureID(int slot) const
{
    if (slot < 0 || slot >= kMaxSlots) {
        return VK_NULL_HANDLE;
    }
    return DescriptorSets[slot];
}
