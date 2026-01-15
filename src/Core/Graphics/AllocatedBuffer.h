#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

struct AllocatedBuffer
{
    VkBuffer buffer=VK_NULL_HANDLE;
    VmaAllocation allocation=nullptr;

    VmaAllocationInfo allocationInfo{};
};

