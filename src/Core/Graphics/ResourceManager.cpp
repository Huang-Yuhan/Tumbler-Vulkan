#include "ResourceManager.h"
#include "CommandManager.h"
#include "Core/Utils/Log.h"
#include "RenderDevice.h"
#include "VulkanUtils.h"

#include <cstring>
#include <fstream>
#include <stb_image.h>
#include <tiny_obj_loader.h>
#include <vk_mem_alloc.h>

namespace Tumbler {

static constexpr VkDeviceSize kVertexBufferSize = 128ULL * 1024 * 1024;
static constexpr VkDeviceSize kIndexBufferSize = 32ULL * 1024 * 1024;

bool ResourceManager::Init(VkDevice device, RenderDevice& renderDevice, CommandManager& commandManager) {
    m_Device = device;
    m_RenderDevice = &renderDevice;
    m_CommandManager = &commandManager;

    // 创建 Unified Vertex Buffer
    VkBufferCreateInfo vbInfo{};
    vbInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vbInfo.size = kVertexBufferSize;
    vbInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VmaAllocationCreateInfo vbAllocInfo{};
    vbAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    auto vbHandle = renderDevice.CreateBuffer(vbInfo, vbAllocInfo, "UnifiedVertexBuffer");
    m_VertexBuffer = vbHandle.Buffer;
    m_VertexBufferAlloc = vbHandle.Allocation;

    // 创建 Unified Index Buffer
    VkBufferCreateInfo ibInfo{};
    ibInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ibInfo.size = kIndexBufferSize;
    ibInfo.usage =
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VmaAllocationCreateInfo ibAllocInfo{};
    ibAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    auto ibHandle = renderDevice.CreateBuffer(ibInfo, ibAllocInfo, "UnifiedIndexBuffer");
    m_IndexBuffer = ibHandle.Buffer;
    m_IndexBufferAlloc = ibHandle.Allocation;

    m_VertexBufferOffset = 0;
    m_IndexBufferOffset = 0;

    LOG_INFO("ResourceManager initialized (VB: {}MB, IB: {}MB)", kVertexBufferSize / (1024 * 1024),
             kIndexBufferSize / (1024 * 1024));
    return true;
}

void ResourceManager::Shutdown() {
    for (auto& view : m_TextureViews) {
        vkDestroyImageView(m_Device, view, nullptr);
    }
    m_TextureViews.clear();

    for (size_t i = 0; i < m_TextureImages.size(); ++i) {
        if (m_TextureImages[i]) {
            vmaDestroyImage(m_RenderDevice->GetAllocator(), m_TextureImages[i], m_TextureAllocations[i]);
        }
    }
    m_TextureAllocations.clear();
    m_TextureImages.clear();
    m_TextureIndexMap.clear();

    if (m_IndexBuffer) {
        vmaDestroyBuffer(m_RenderDevice->GetAllocator(), m_IndexBuffer, m_IndexBufferAlloc);
        m_IndexBuffer = VK_NULL_HANDLE;
    }
    if (m_VertexBuffer) {
        vmaDestroyBuffer(m_RenderDevice->GetAllocator(), m_VertexBuffer, m_VertexBufferAlloc);
        m_VertexBuffer = VK_NULL_HANDLE;
    }
}

MeshHandle ResourceManager::UploadMesh(const std::string& objPath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, objPath.c_str())) {
        LOG_ERROR("Failed to load OBJ: {}", objPath);
        return {};
    }

    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    glm::vec3 minPos(std::numeric_limits<float>::max());
    glm::vec3 maxPos(std::numeric_limits<float>::lowest());

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            // Position
            vertices.push_back(attrib.vertices[3 * index.vertex_index + 0]);
            vertices.push_back(attrib.vertices[3 * index.vertex_index + 1]);
            vertices.push_back(attrib.vertices[3 * index.vertex_index + 2]);

            glm::vec3 pos(attrib.vertices[3 * index.vertex_index + 0], attrib.vertices[3 * index.vertex_index + 1],
                          attrib.vertices[3 * index.vertex_index + 2]);
            minPos = glm::min(minPos, pos);
            maxPos = glm::max(maxPos, pos);

            // Normal
            if (index.normal_index >= 0) {
                vertices.push_back(attrib.normals[3 * index.normal_index + 0]);
                vertices.push_back(attrib.normals[3 * index.normal_index + 1]);
                vertices.push_back(attrib.normals[3 * index.normal_index + 2]);
            } else {
                vertices.push_back(0.0f);
                vertices.push_back(0.0f);
                vertices.push_back(1.0f);
            }

            // TexCoord
            if (index.texcoord_index >= 0) {
                vertices.push_back(attrib.texcoords[2 * index.texcoord_index + 0]);
                vertices.push_back(1.0f - attrib.texcoords[2 * index.texcoord_index + 1]);
            } else {
                vertices.push_back(0.0f);
                vertices.push_back(1.0f);
            }

            indices.push_back(static_cast<uint32_t>(indices.size()));
        }
    }

    uint32_t vertexSize = static_cast<uint32_t>(vertices.size() * sizeof(float));
    uint32_t indexSize = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));

    // Sub-allocation 检查容量
    if (m_VertexBufferOffset + vertexSize > kVertexBufferSize || m_IndexBufferOffset + indexSize > kIndexBufferSize) {
        LOG_ERROR("Unified buffer out of memory");
        return {};
    }

    MeshHandle handle;
    handle.VertexOffset = m_VertexBufferOffset;
    handle.IndexOffset = m_IndexBufferOffset;
    handle.VertexCount = static_cast<uint32_t>(vertices.size() / 8); // pos(3) + normal(3) + uv(2) = 8 floats
    handle.IndexCount = static_cast<uint32_t>(indices.size());

    // Bounding sphere
    glm::vec3 center = (minPos + maxPos) * 0.5f;
    float radius = glm::length(maxPos - minPos) * 0.5f;
    handle.BoundingSphere = glm::vec4(center, radius);

    uint32_t stagingSize = vertexSize + indexSize;
    VkBufferCreateInfo stagingInfo{};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = stagingSize;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo stagingAllocInfo{};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    auto stagingHandle = m_RenderDevice->CreateBuffer(stagingInfo, stagingAllocInfo, "MeshStaging");

    void* data;
    vmaMapMemory(m_RenderDevice->GetAllocator(), stagingHandle.Allocation, &data);
    memcpy(data, vertices.data(), vertexSize);
    memcpy(static_cast<uint8_t*>(data) + vertexSize, indices.data(), indexSize);
    vmaUnmapMemory(m_RenderDevice->GetAllocator(), stagingHandle.Allocation);

    // 通过 staging buffer 上传
    m_CommandManager->ImmediateSubmit([&](VkCommandBuffer cmd) {
        // Copy to unified VB
        VkBufferCopy vbCopy{};
        vbCopy.srcOffset = 0;
        vbCopy.dstOffset = handle.VertexOffset;
        vbCopy.size = vertexSize;
        vkCmdCopyBuffer(cmd, stagingHandle.Buffer, m_VertexBuffer, 1, &vbCopy);

        // Copy to unified IB
        VkBufferCopy ibCopy{};
        ibCopy.srcOffset = vertexSize;
        ibCopy.dstOffset = handle.IndexOffset;
        ibCopy.size = indexSize;
        vkCmdCopyBuffer(cmd, stagingHandle.Buffer, m_IndexBuffer, 1, &ibCopy);
    });
    m_RenderDevice->DestroyBuffer(stagingHandle);

    m_VertexBufferOffset += vertexSize;
    m_IndexBufferOffset += indexSize;

    LOG_INFO("Mesh uploaded: {} (vertices: {}, indices: {})", objPath, handle.VertexCount, handle.IndexCount);
    return handle;
}

void ResourceManager::DestroyMesh(const MeshHandle& handle) {
    // Sub-allocation 不支持回收单个 mesh
    // Mesh 删除时只是弃用，空间不回收
    (void)handle;
}

TextureHandle ResourceManager::UploadTexture(const std::string& path) {
    // Check if already loaded
    auto it = m_TextureIndexMap.find(path);
    if (it != m_TextureIndexMap.end()) {
        TextureHandle handle;
        handle.Image = m_TextureImages[it->second];
        handle.ImageView = m_TextureViews[it->second];
        handle.TextureIndex = it->second;
        return handle;
    }

    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) {
        LOG_ERROR("Failed to load texture: {}", path);
        return {};
    }

    uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    // Staging buffer
    VkBufferCreateInfo stagingInfo{};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = imageSize;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo stagingAllocInfo{};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    auto stagingHandle = m_RenderDevice->CreateBuffer(stagingInfo, stagingAllocInfo, "TextureStaging");

    void* data;
    vmaMapMemory(m_RenderDevice->GetAllocator(), stagingHandle.Allocation, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vmaUnmapMemory(m_RenderDevice->GetAllocator(), stagingHandle.Allocation);
    stbi_image_free(pixels);

    // Create device-local image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.extent = {static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1};
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo imageAllocInfo{};
    imageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkImage image;
    VmaAllocation imageAlloc;
    VK_CHECK(vmaCreateImage(m_RenderDevice->GetAllocator(), &imageInfo, &imageAllocInfo, &image, &imageAlloc, nullptr));

    // Upload + transition
    m_CommandManager->ImmediateSubmit([&](VkCommandBuffer cmd) {
        m_CommandManager->TransitionImageLayout(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1};

        vkCmdCopyBufferToImage(cmd, stagingHandle.Buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        // Generate mipmaps via blit
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        int32_t mipWidth = texWidth;
        int32_t mipHeight = texHeight;

        for (uint32_t i = 1; i < mipLevels; i++) {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                                 nullptr, 1, &barrier);

            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                                 nullptr, 0, nullptr, 1, &barrier);

            if (mipWidth > 1)
                mipWidth /= 2;
            if (mipHeight > 1)
                mipHeight /= 2;
        }

        // Last mip level transition
        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                             0, nullptr, 1, &barrier);
    });

    // Cleanup staging buffer
    vmaDestroyBuffer(m_RenderDevice->GetAllocator(), stagingHandle.Buffer, stagingHandle.Allocation);

    // Create ImageView
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    VK_CHECK(vkCreateImageView(m_Device, &viewInfo, nullptr, &imageView));

    uint32_t textureIndex = static_cast<uint32_t>(m_TextureViews.size());
    m_TextureIndexMap[path] = textureIndex;
    m_TextureImages.push_back(image);
    m_TextureViews.push_back(imageView);
    m_TextureAllocations.push_back(imageAlloc);

    LOG_INFO("Texture uploaded: {} ({}x{}, mips: {}, index: {})", path, texWidth, texHeight, mipLevels, textureIndex);

    TextureHandle handle;
    handle.Image = image;
    handle.ImageView = imageView;
    handle.TextureIndex = textureIndex;
    return handle;
}

void ResourceManager::DestroyTexture(const TextureHandle& handle) {
    // Textures are managed by index; DestroyTexture is a no-op for now
    // Individual texture cleanup is done via index during Shutdown
    (void)handle;
}

VkShaderModule ResourceManager::LoadShader(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open shader file: {}", path);
        return VK_NULL_HANDLE;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = fileSize;
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule));

    LOG_INFO("Shader loaded: {}", path);
    return shaderModule;
}

void ResourceManager::DestroyShaderModule(VkShaderModule module) {
    if (module) {
        vkDestroyShaderModule(m_Device, module, nullptr);
    }
}

} // namespace Tumbler
