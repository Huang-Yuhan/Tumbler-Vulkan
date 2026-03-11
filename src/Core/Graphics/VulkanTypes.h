#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>

// 之前定义的 Buffer 结构
struct AllocatedBuffer {
    VkBuffer Buffer = VK_NULL_HANDLE;
    VmaAllocation Allocation = VK_NULL_HANDLE;
    VmaAllocationInfo Info{};
};

// 【新增】移动到这里：通用图片资源结构
struct AllocatedImage {
    VkImage Image = VK_NULL_HANDLE;
    VkImageView ImageView = VK_NULL_HANDLE;
    VmaAllocation Allocation = VK_NULL_HANDLE;
};

struct FSceneData {
    glm::mat4 ViewProjection;
    glm::vec4 CameraPosition;
    glm::vec4 LightPosition;
    glm::vec4 LightColor;
};
