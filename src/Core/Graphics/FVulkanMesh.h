#pragma once

#include "AllocatedBuffer.h"
#include "vulkan/vulkan.h"

class FVulkanMesh
{
    AllocatedBuffer VertexBuffer{};
    AllocatedBuffer IndexBuffer{};

    uint32_t IndexCount = 0;

    void Destroy(VmaAllocator allocator);
};