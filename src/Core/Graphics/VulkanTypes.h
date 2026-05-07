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

// GPU-side light data (matches shader LightGPUData)
// Position.w = ELightType cast to float (0=Point, 1=Directional)
// Color.w    = Intensity
// Direction.xyz = light direction (directional), Direction.w = Range (point)
struct LightGPUData {
    glm::vec4 Position;
    glm::vec4 Color;
    glm::vec4 Direction;
};

struct SceneDataUBO {
    glm::mat4 ViewProjection;
    glm::mat4 InvViewProj;
    glm::vec4 CameraPosition;
    LightGPUData Lights[MAX_SCENE_LIGHTS];
    int LightCount;
    int padding[3];
    glm::mat4 LightViewProj;
};
