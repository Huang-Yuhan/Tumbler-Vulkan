#include "DescriptorManager.h"
#include "RenderDevice.h"
#include <array>
#include <cstring>
#include <stdexcept>
#include <utility>

void DescriptorManager::Init(VkDevice device, RenderDevice* renderDevice)
{
    // Create descriptor pool
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = kMaxDescriptorSets;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = kMaxDescriptorSets;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = kMaxDescriptorSets;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &Pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Descriptor Pool");
    }

    // Create global descriptor set layout
    // binding=0: SceneDataUBO, binding=1: Shadow map sampler
    VkDescriptorSetLayoutBinding bindings[2]{};

    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &GlobalSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Global Descriptor Set Layout");
    }

    // Allocate global descriptor set
    GlobalDescriptorSet = AllocateDescriptorSet(device, GlobalSetLayout);

    // Create scene parameter UBO
    renderDevice->CreateBuffer(
        sizeof(SceneDataUBO),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
        SceneParameterBuffer);

    // Write descriptor: bind UBO to set=0, binding=0
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = SceneParameterBuffer.Buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(SceneDataUBO);

    VkWriteDescriptorSet setWrite{};
    setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    setWrite.dstSet = GlobalDescriptorSet;
    setWrite.dstBinding = 0;
    setWrite.descriptorCount = 1;
    setWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    setWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device, 1, &setWrite, 0, nullptr);
}

void DescriptorManager::Cleanup(VkDevice device, RenderDevice* renderDevice)
{
    if (renderDevice && SceneParameterBuffer.Buffer != VK_NULL_HANDLE) {
        renderDevice->DestroyBuffer(SceneParameterBuffer);
    }

    if (GlobalSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, std::exchange(GlobalSetLayout, VK_NULL_HANDLE), nullptr);
    }
    if (Pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, std::exchange(Pool, VK_NULL_HANDLE), nullptr);
    }
}

VkDescriptorSet DescriptorManager::AllocateDescriptorSet(VkDevice device, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = Pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet descriptorSet;
    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate Descriptor Set");
    }
    return descriptorSet;
}

void DescriptorManager::QueueDescriptorSetFree(VkDescriptorSet descriptorSet)
{
    PendingFrees.Enqueue(descriptorSet);
}

void DescriptorManager::FlushPendingDescriptorSetFrees(VkDevice device)
{
    if (PendingFrees.Empty() || Pool == VK_NULL_HANDLE) return;

    const auto& pending = PendingFrees.GetPendingDescriptorSets();
    vkFreeDescriptorSets(device, Pool,
        static_cast<uint32_t>(pending.size()), pending.data());
    PendingFrees.Clear();
}

void DescriptorManager::UpdateShadowBinding(VkDevice device, VkSampler sampler, VkImageView shadowMapView)
{
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = sampler;
    imageInfo.imageView = shadowMapView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = GlobalDescriptorSet;
    write.dstBinding = 1;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}
