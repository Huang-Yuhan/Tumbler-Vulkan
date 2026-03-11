#pragma once

#include "VulkanContext.h"
#include "VulkanSwapchain.h"
#include "FVulkanMesh.h"
#include "VulkanTypes.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <string>
#include <glm/glm.hpp>

#include "Core/Assets/TextureManager.h"

class FTexture;
class AppWindow;
class FMesh;
class FScene;
class CCamera;
class CTransform;

class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    void Init(AppWindow* window);
    void Cleanup();

    // 渲染接口：传入场景和相机
    void Render(const FScene* scene, const CCamera* camera, const CTransform* cameraTransform);
    FVulkanMesh& UploadMesh(FMesh* cpuMesh);

    [[nodiscard]] VkDevice GetDevice() const { return Context.GetDevice(); }
    [[nodiscard]] TextureManager* GetTextureManager() const { return TexManager.get(); }
    [[nodiscard]] VkDescriptorSetLayout GetGlobalSetLayout() const { return GlobalSetLayout; }
    std::shared_ptr<FTexture> LoadTexture(const std::string& filePath);

    // ==========================================
    // 【新增】公开给 FMaterial / FMaterialInstance 使用的接口
    // ==========================================
    [[nodiscard]] VkRenderPass GetRenderPass() const { return RenderPass; }
    [[nodiscard]] VkExtent2D GetSwapchainExtent() const { return SwapChain.GetExtent(); }

    bool LoadShaderModule(const char* filePath, VkShaderModule* outShaderModule);
    void CreateBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, AllocatedBuffer& outBuffer);
    void DestroyBuffer(AllocatedBuffer& buffer);
    VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout);

private:
    VulkanContext Context;
    VulkanSwapchain SwapChain;

    VkRenderPass RenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> Framebuffers;

    VkCommandPool CommandPool = VK_NULL_HANDLE;
    VkCommandBuffer MainCommandBuffer = VK_NULL_HANDLE;

    VkSemaphore ImageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore RenderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence RenderFence = VK_NULL_HANDLE;
    VkFence UploadFence = VK_NULL_HANDLE;

    std::unordered_map<FMesh*, FVulkanMesh> MeshCache;
    std::unique_ptr<TextureManager> TexManager;

    // 【修改】只保留池子，删除具体的 Layout 和 Pipeline
    VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;

    VkDescriptorSetLayout GlobalSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet GlobalDescriptorSet = VK_NULL_HANDLE;
    AllocatedBuffer SceneParameterBuffer{};

    // 内部函数
    void InitRenderPass();
    void InitFramebuffers();
    void InitCommands();
    void InitSyncStructures();
    void InitUploadSync();
    void InitDescriptors(); // 这个还在，但内容变了

    // 录制命令缓冲
    void RecordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, const FScene* scene, const CCamera* camera, const CTransform* cameraTransform);
    void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
};