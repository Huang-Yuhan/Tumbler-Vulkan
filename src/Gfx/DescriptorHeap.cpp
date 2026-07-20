#include "DescriptorHeap.h"
#include "Core/Utils/Log.h"

#include <array>

namespace Tumbler {

std::expected<void, DescriptorError> DescriptorHeap::Init(VkDevice device, uint32_t maxBindlessTextures) {
    m_Device = device;
    m_MaxTextures = maxBindlessTextures;

    // ---- Default sampler ----
    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .maxLod = VK_LOD_CLAMP_NONE,
    };
    vkCreateSampler(m_Device, &samplerInfo, nullptr, &m_DefaultSampler);

    // ---- Set 0 Layout: Global ----
    std::array<VkDescriptorSetLayoutBinding, 2> set0Bindings{{
        {
            .binding = Bindings::SceneUBO,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = Bindings::ShadowMap,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
    }};

    VkDescriptorSetLayoutCreateInfo set0LayoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(set0Bindings.size()),
        .pBindings = set0Bindings.data(),
    };

    if (vkCreateDescriptorSetLayout(m_Device, &set0LayoutInfo, nullptr, &m_Set0Layout) != VK_SUCCESS) {
        LOG_ERROR("Failed to create Set 0 layout");
        return std::unexpected(DescriptorError::LayoutCreationFailed);
    }

    // ---- Set 1 Layout: Bindless ----
    VkDescriptorBindingFlags bindlessFlags =
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    VkDescriptorBindingFlags arrayFlags =
        bindlessFlags | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 1,
        .pBindingFlags = &arrayFlags,
    };

    std::array<VkDescriptorSetLayoutBinding, 3> set1Bindings{{
        {
            .binding = Bindings::Textures,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = maxBindlessTextures,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = Bindings::MaterialData,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = Bindings::ClusterPageData,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
    }};

    VkDescriptorSetLayoutCreateInfo set1LayoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &bindingFlagsInfo,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = static_cast<uint32_t>(set1Bindings.size()),
        .pBindings = set1Bindings.data(),
    };

    if (vkCreateDescriptorSetLayout(m_Device, &set1LayoutInfo, nullptr, &m_Set1Layout) != VK_SUCCESS) {
        LOG_ERROR("Failed to create Set 1 layout");
        return std::unexpected(DescriptorError::LayoutCreationFailed);
    }

    // ---- Descriptor Pool ----
    std::array<VkDescriptorPoolSize, 5> poolSizes{{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxBindlessTextures + 2},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 8},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 8},
    }};

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = 4,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    if (vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_Pool) != VK_SUCCESS) {
        LOG_ERROR("Failed to create descriptor pool");
        return std::unexpected(DescriptorError::PoolCreationFailed);
    }

    // ---- Allocate sets ----
    VkDescriptorSetAllocateInfo setAlloc{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_Pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_Set0Layout,
    };
    vkAllocateDescriptorSets(m_Device, &setAlloc, &m_Set0);

    // Set 1 uses variable count
    uint32_t maxCount = maxBindlessTextures;
    VkDescriptorSetVariableDescriptorCountAllocateInfo varInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &maxCount,
    };

    VkDescriptorSetAllocateInfo set1Alloc{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &varInfo,
        .descriptorPool = m_Pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_Set1Layout,
    };
    vkAllocateDescriptorSets(m_Device, &set1Alloc, &m_Set1);

    LOG_INFO("DescriptorHeap initialized ({} bindless textures)", maxBindlessTextures);
    return {};
}

void DescriptorHeap::Shutdown() {
    if (m_DefaultSampler) { vkDestroySampler(m_Device, m_DefaultSampler, nullptr); m_DefaultSampler = VK_NULL_HANDLE; }
    if (m_Pool)           { vkDestroyDescriptorPool(m_Device, m_Pool, nullptr); m_Pool = VK_NULL_HANDLE; }
    if (m_Set0Layout)     { vkDestroyDescriptorSetLayout(m_Device, m_Set0Layout, nullptr); m_Set0Layout = VK_NULL_HANDLE; }
    if (m_Set1Layout)     { vkDestroyDescriptorSetLayout(m_Device, m_Set1Layout, nullptr); m_Set1Layout = VK_NULL_HANDLE; }
    LOG_INFO("DescriptorHeap shutdown");
}

std::expected<VkPipelineLayout, DescriptorError> DescriptorHeap::CreatePipelineLayout(
    VkDevice device, std::span<const VkPushConstantRange> pushConstants) {
    VkDescriptorSetLayout layouts[] = {m_Set0Layout, m_Set1Layout};

    VkPipelineLayoutCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 2,
        .pSetLayouts = layouts,
        .pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size()),
        .pPushConstantRanges = pushConstants.data(),
    };

    VkPipelineLayout layout;
    if (vkCreatePipelineLayout(device, &info, nullptr, &layout) != VK_SUCCESS) {
        LOG_ERROR("Failed to create pipeline layout");
        return std::unexpected(DescriptorError::LayoutCreationFailed);
    }
    return layout;
}

void DescriptorHeap::WriteSet0(VkBuffer sceneUbo, VkImageView shadowMap, VkSampler shadowSampler) {
    VkDescriptorBufferInfo uboInfo{
        .buffer = sceneUbo,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };

    VkDescriptorImageInfo shadowInfo{
        .sampler = shadowSampler ? shadowSampler : m_DefaultSampler,
        .imageView = shadowMap ? shadowMap : VK_NULL_HANDLE,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
    };

    std::vector<VkWriteDescriptorSet> writes;

    writes.push_back({
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_Set0,
        .dstBinding = Bindings::SceneUBO,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &uboInfo,
    });

    writes.push_back({
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_Set0,
        .dstBinding = Bindings::ShadowMap,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &shadowInfo,
    });

    vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

uint32_t DescriptorHeap::RegisterTexture(VkImageView view, VkSampler sampler) {
    uint32_t idx = m_NextTextureIndex++;
    if (idx >= m_MaxTextures) {
        LOG_ERROR("Bindless texture array full (max {})", m_MaxTextures);
        return ~0u;
    }

    if (!sampler) sampler = m_DefaultSampler;

    VkDescriptorImageInfo imageInfo{
        .sampler = sampler,
        .imageView = view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_Set1,
        .dstBinding = Bindings::Textures,
        .dstArrayElement = idx,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageInfo,
    };
    vkUpdateDescriptorSets(m_Device, 1, &write, 0, nullptr);

    return idx;
}

} // namespace Tumbler
