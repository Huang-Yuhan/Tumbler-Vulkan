#pragma once

#include "AllocatedBuffer.h"
#include "vulkan/vulkan.h"

struct FVulkanMesh
{
    AllocatedBuffer VertexBuffer{};
    AllocatedBuffer IndexBuffer{};

    uint32_t IndexCount = 0;
};