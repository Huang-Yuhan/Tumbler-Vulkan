//
// Created by Icecream_Sarkaz on 2026/1/15.
//

#include "FVulkanMesh.h"

void FVulkanMesh::Destroy(VmaAllocator allocator)
{
    if (VertexBuffer.buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, VertexBuffer.buffer, VertexBuffer.allocation);
        VertexBuffer.buffer = VK_NULL_HANDLE;
        VertexBuffer.allocation = nullptr;
    }
    if (IndexBuffer.buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, IndexBuffer.buffer, IndexBuffer.allocation);
        IndexBuffer.buffer = VK_NULL_HANDLE;
        IndexBuffer.allocation = nullptr;
    }
}