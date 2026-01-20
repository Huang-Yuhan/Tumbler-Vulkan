#pragma once

#include <Core/Graphics/VulkanTypes.h>
#include "vulkan/vulkan.h"

struct FVulkanMesh
{
    AllocatedBuffer VertexBuffer{};
    AllocatedBuffer IndexBuffer{};

    uint32_t IndexCount = 0;
};