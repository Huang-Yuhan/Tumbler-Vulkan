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

#define MAX_SCENE_LIGHTS 8

struct PointLightData {
    glm::vec4 Position; // w is padding or radius
    glm::vec4 Color;    // w is Intensity
};

struct SceneDataUBO {
    glm::mat4 ViewProjection;
    glm::vec4 CameraPosition;
    PointLightData Lights[MAX_SCENE_LIGHTS];
    int LightCount;
    int padding[3];
};
