#include "Core/Graphics/ResourceUploadManager.h"
#include "Core/Graphics/RenderDevice.h"
#include "Core/Graphics/CommandBufferManager.h"
#include "Core/Geometry/FMesh.h"
#include "Core/Graphics/FVulkanMesh.h"
#include "Core/Assets/FTexture.h"
#include "Core/Utils/Log.h"
#include "Core/Utils/VulkanUtils.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fstream>
#include <vector>

ResourceUploadManager::ResourceUploadManager() = default;

ResourceUploadManager::~ResourceUploadManager() {
    Cleanup();
}

ResourceUploadManager::ResourceUploadManager(ResourceUploadManager&& other) noexcept
    : RenderDeviceRef(other.RenderDeviceRef)
    , CommandBufferManagerRef(other.CommandBufferManagerRef)
    , Device(other.Device)
    , MeshCache(std::move(other.MeshCache)) {
    other.RenderDeviceRef = nullptr;
    other.CommandBufferManagerRef = nullptr;
    other.Device = VK_NULL_HANDLE;
}

ResourceUploadManager& ResourceUploadManager::operator=(ResourceUploadManager&& other) noexcept {
    if (this != &other) {
        Cleanup();
        RenderDeviceRef = other.RenderDeviceRef;
        CommandBufferManagerRef = other.CommandBufferManagerRef;
        Device = other.Device;
        MeshCache = std::move(other.MeshCache);
        other.RenderDeviceRef = nullptr;
        other.CommandBufferManagerRef = nullptr;
        other.Device = VK_NULL_HANDLE;
    }
    return *this;
}

void ResourceUploadManager::Init(RenderDevice* renderDevice, CommandBufferManager* commandBufferManager) {
    RenderDeviceRef = renderDevice;
    CommandBufferManagerRef = commandBufferManager;
    Device = renderDevice->GetDevice();

    LOG_INFO("ResourceUploadManager initialized");
}

void ResourceUploadManager::Cleanup() {
    // 清理所有 Mesh 缓存
    for (auto& [cpuMesh, gpuMesh] : MeshCache) {
        RenderDeviceRef->DestroyBuffer(gpuMesh.VertexBuffer);
        RenderDeviceRef->DestroyBuffer(gpuMesh.IndexBuffer);
    }
    MeshCache.clear();

    RenderDeviceRef = nullptr;
    CommandBufferManagerRef = nullptr;
    Device = VK_NULL_HANDLE;
}

FVulkanMesh& ResourceUploadManager::UploadMesh(FMesh* cpuMesh) {
    if (MeshCache.find(cpuMesh) != MeshCache.end()) {
        return MeshCache[cpuMesh];
    }

    FVulkanMesh gpuMesh;
    const auto& indices = cpuMesh->GetIndices();
    gpuMesh.IndexCount = static_cast<uint32_t>(indices.size());

    const auto& rawVertexData = cpuMesh->GetRawVertexData();
    size_t vSize = rawVertexData.size();

    // 创建 Staging Buffer
    AllocatedBuffer vStaging;
    RenderDeviceRef->CreateBuffer(vSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, vStaging);
    memcpy(vStaging.Info.pMappedData, rawVertexData.data(), vSize);

    // 创建 GPU Buffer
    RenderDeviceRef->CreateBuffer(
        vSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        gpuMesh.VertexBuffer
    );

    size_t iSize = indices.size() * sizeof(uint32_t);
    AllocatedBuffer iStaging;
    RenderDeviceRef->CreateBuffer(iSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, iStaging);
    memcpy(iStaging.Info.pMappedData, indices.data(), iSize);

    RenderDeviceRef->CreateBuffer(
        iSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        gpuMesh.IndexBuffer
    );

    // 上传数据
    CommandBufferManagerRef->ImmediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy copyRegion{};
        copyRegion.size = vSize;
        vkCmdCopyBuffer(cmd, vStaging.Buffer, gpuMesh.VertexBuffer.Buffer, 1, &copyRegion);
        copyRegion.size = iSize;
        vkCmdCopyBuffer(cmd, iStaging.Buffer, gpuMesh.IndexBuffer.Buffer, 1, &copyRegion);
    });

    // 清理 Staging Buffer
    RenderDeviceRef->DestroyBuffer(vStaging);
    RenderDeviceRef->DestroyBuffer(iStaging);

    MeshCache[cpuMesh] = gpuMesh;
    LOG_INFO("Uploaded Mesh. RawBytes: {}, Indices: {}", vSize, gpuMesh.IndexCount);
    return MeshCache[cpuMesh];
}

bool ResourceUploadManager::IsMeshUploaded(FMesh* cpuMesh) const {
    return MeshCache.find(cpuMesh) != MeshCache.end();
}

FVulkanMesh& ResourceUploadManager::GetUploadedMesh(FMesh* cpuMesh) {
    return MeshCache[cpuMesh];
}

void ResourceUploadManager::ClearMeshCache() {
    for (auto& [cpuMesh, gpuMesh] : MeshCache) {
        RenderDeviceRef->DestroyBuffer(gpuMesh.VertexBuffer);
        RenderDeviceRef->DestroyBuffer(gpuMesh.IndexBuffer);
    }
    MeshCache.clear();
}

std::shared_ptr<FTexture> ResourceUploadManager::LoadTexture(const std::string& filePath) {
    // 使用 stb_image 加载文件
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels) {
        // 简单的备用路径查找
        std::string fallbackPath = "../../" + filePath;
        pixels = stbi_load(fallbackPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    }

    if (!pixels) {
        LOG_ERROR("Failed to load texture: {}", filePath);
        throw std::runtime_error("Texture load failed");
    }

    auto texture = CreateTextureFromMemory(pixels, texWidth, texHeight, 4);
    stbi_image_free(pixels);

    LOG_INFO("Texture loaded from file: {} ({}x{})", filePath, texWidth, texHeight);
    return texture;
}

std::shared_ptr<FTexture> ResourceUploadManager::CreateTextureFromMemory(
    const unsigned char* pixels,
    int width,
    int height,
    int channels) {

    VkDeviceSize imageSize = width * height * 4;

    // 创建 Staging Buffer
    AllocatedBuffer stagingBuffer;
    RenderDeviceRef->CreateBuffer(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
        stagingBuffer
    );
    memcpy(stagingBuffer.Info.pMappedData, pixels, static_cast<size_t>(imageSize));

    // 创建 Image
    AllocatedImage newImage;
    RenderDeviceRef->CreateImage(
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        newImage
    );

    // 转换布局并上传
    CommandBufferManagerRef->TransitionImageLayout(
        newImage.Image,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    CommandBufferManagerRef->CopyBufferToImage(
        stagingBuffer.Buffer,
        newImage.Image,
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    );

    CommandBufferManagerRef->TransitionImageLayout(
        newImage.Image,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    // 清理 Staging Buffer
    RenderDeviceRef->DestroyBuffer(stagingBuffer);

    // 创建 ImageView
    newImage.ImageView = RenderDeviceRef->CreateImageView(
        newImage.Image,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    // 创建 Sampler
    VkSampler sampler = RenderDeviceRef->CreateSampler(
        VK_FILTER_LINEAR,
        VK_FILTER_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        true
    );

    // 创建 FTexture 对象
    return std::make_shared<FTexture>(RenderDeviceRef->GetContext(), newImage, sampler);
}

bool ResourceUploadManager::LoadShaderModule(const char* filePath, VkShaderModule* outShaderModule) {
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        LOG_ERROR("Failed to load shader file: {}", filePath);
        return false;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    if (vkCreateShaderModule(Device, &createInfo, nullptr, outShaderModule) != VK_SUCCESS) {
        return false;
    }
    return true;
}
