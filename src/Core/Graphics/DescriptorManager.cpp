#include "DescriptorManager.h"
#include "Core/Utils/Log.h"
#include "VulkanUtils.h"

namespace Tumbler {

bool DescriptorManager::Init(VkDevice device, uint32_t maxTextures) {
    m_Device = device;
    m_MaxTextures = maxTextures;

    // --- DescriptorPool (统一大池) ---
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, maxTextures * 2},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 20},
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags =
        VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolInfo.maxSets = 50;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_Pool));

    // --- Set 0 Layout: Global (SceneUBO + ShadowMap) ---
    VkDescriptorSetLayoutBinding set0Binding0{};
    set0Binding0.binding = 0;
    set0Binding0.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    set0Binding0.descriptorCount = 1;
    set0Binding0.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding set0Binding1{};
    set0Binding1.binding = 1;
    set0Binding1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set0Binding1.descriptorCount = 1;
    set0Binding1.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding set0Bindings[] = {set0Binding0, set0Binding1};

    VkDescriptorSetLayoutCreateInfo set0LayoutInfo{};
    set0LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set0LayoutInfo.bindingCount = 2;
    set0LayoutInfo.pBindings = set0Bindings;

    VK_CHECK(vkCreateDescriptorSetLayout(device, &set0LayoutInfo, nullptr, &m_Set0Layout));

    // --- Set 1 Layout: Bindless (texture2D[] + MaterialData + ObjectData) ---
    VkDescriptorSetLayoutBinding set1Binding0{};
    set1Binding0.binding = 0;
    set1Binding0.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    set1Binding0.descriptorCount = maxTextures;
    set1Binding0.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding set1Binding1{};
    set1Binding1.binding = 1;
    set1Binding1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    set1Binding1.descriptorCount = 1;
    set1Binding1.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding set1Binding2{};
    set1Binding2.binding = 2;
    set1Binding2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    set1Binding2.descriptorCount = 1;
    set1Binding2.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding set1Bindings[] = {set1Binding0, set1Binding1, set1Binding2};

    VkDescriptorSetLayoutCreateInfo set1LayoutInfo{};
    set1LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    VkDescriptorBindingFlags bindlessFlags[] = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        0,
        0,
    };
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
    bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsInfo.bindingCount = 3;
    bindingFlagsInfo.pBindingFlags = bindlessFlags;

    set1LayoutInfo.pNext = &bindingFlagsInfo;
    set1LayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    set1LayoutInfo.bindingCount = 3;
    set1LayoutInfo.pBindings = set1Bindings;

    VK_CHECK(vkCreateDescriptorSetLayout(device, &set1LayoutInfo, nullptr, &m_Set1Layout));

    LOG_INFO("DescriptorManager initialized (max textures: {})", maxTextures);
    return true;
}

void DescriptorManager::Shutdown() {
    if (m_Set1Layout) {
        vkDestroyDescriptorSetLayout(m_Device, m_Set1Layout, nullptr);
        m_Set1Layout = VK_NULL_HANDLE;
    }
    if (m_Set0Layout) {
        vkDestroyDescriptorSetLayout(m_Device, m_Set0Layout, nullptr);
        m_Set0Layout = VK_NULL_HANDLE;
    }
    if (m_Pool) {
        vkDestroyDescriptorPool(m_Device, m_Pool, nullptr);
        m_Pool = VK_NULL_HANDLE;
    }
}

VkDescriptorSet DescriptorManager::AllocateSet(VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_Pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateDescriptorSets(m_Device, &allocInfo, &set));
    return set;
}

void DescriptorManager::FreeSet(VkDescriptorSet set) {
    vkFreeDescriptorSets(m_Device, m_Pool, 1, &set);
}

void DescriptorManager::UpdateSet0(VkDescriptorSet set, VkBuffer sceneUBO, VkImageView shadowMapView,
                                   VkSampler shadowSampler) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = sceneUBO;
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = shadowMapView;
    imageInfo.sampler = shadowSampler;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writes[2]{};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = set;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &bufferInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = set;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_Device, 2, writes, 0, nullptr);
}

void DescriptorManager::UpdateSet1Bindless(VkDescriptorSet set, const std::vector<VkImageView>& textureViews,
                                           VkBuffer materialSSBO, VkBuffer objectSSBO) {
    std::vector<VkDescriptorImageInfo> imageInfos;
    imageInfos.reserve(textureViews.size());
    for (auto view : textureViews) {
        VkDescriptorImageInfo info{};
        info.imageView = view;
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos.push_back(info);
    }

    VkDescriptorBufferInfo materialInfo{};
    materialInfo.buffer = materialSSBO;
    materialInfo.offset = 0;
    materialInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo objectInfo{};
    objectInfo.buffer = objectSSBO;
    objectInfo.offset = 0;
    objectInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet writes[3]{};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = set;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[0].descriptorCount = static_cast<uint32_t>(imageInfos.size());
    writes[0].pImageInfo = imageInfos.data();

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = set;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &materialInfo;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = set;
    writes[2].dstBinding = 2;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[2].descriptorCount = 1;
    writes[2].pBufferInfo = &objectInfo;

    vkUpdateDescriptorSets(m_Device, 3, writes, 0, nullptr);
}

} // namespace Tumbler
