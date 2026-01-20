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
class FMesh; // 前置声明

struct MeshPushConstants {
    glm::vec4 data;
    glm::mat4 render_matrix;
};

class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    void Init(AppWindow* window);
    void Cleanup();

    // 修改：允许传入 Mesh 指针进行绘制
    void Render(FVulkanMesh* mesh, const glm::mat4& transformMatrix);
    FVulkanMesh& UploadMesh(FMesh* cpuMesh);

    [[nodiscard]] VkDevice GetDevice() const { return Context.GetDevice(); }

    // 让外部能获取到 TextureManager (比如逻辑层要预加载纹理)
    [[nodiscard]] TextureManager* GetTextureManager() const { return TexManager.get(); }

    // 【修改】LoadTexture 依然保留，但变成 public 或 friend，供 Manager 调用
    // 建议把返回值改为 shared_ptr 以配合 Manager
    std::shared_ptr<FTexture> LoadTexture(const std::string& filePath);

private:
    VulkanContext Context;
    VulkanSwapchain Swapchain;

    VkRenderPass RenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> Framebuffers;

    VkCommandPool CommandPool = VK_NULL_HANDLE;
    VkCommandBuffer MainCommandBuffer = VK_NULL_HANDLE;

    VkSemaphore ImageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore RenderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence RenderFence = VK_NULL_HANDLE;

    // 数据上传相关
    VkFence UploadFence = VK_NULL_HANDLE;
    std::unordered_map<FMesh*, FVulkanMesh> MeshCache;

    // 管线相关
    VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
    VkPipeline GraphicsPipeline = VK_NULL_HANDLE;


    // 改为持有 Manager
    std::unique_ptr<TextureManager> TexManager;

    // 我们可能需要一个变量来暂时存一下当前演示用的纹理
    std::shared_ptr<FTexture> CurrentDemoTexture;


    VkDescriptorSetLayout DescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet TextureDescriptorSet = VK_NULL_HANDLE;


    // ==========================================
    // 内部函数
    // ==========================================
    void InitRenderPass();
    void InitFramebuffers();
    void InitCommands();
    void InitSyncStructures();
    void InitUploadSync();
    void InitGraphicsPipeline(); // 初始化管线

    bool LoadShaderModule(const char* filePath, VkShaderModule* outShaderModule);

    // 修改：Record 接收 Mesh 参数
    void RecordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, FVulkanMesh* mesh, const glm::mat4& matrix);
    void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
    void CreateBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, AllocatedBuffer& outBuffer);
    void DestroyBuffer(AllocatedBuffer& buffer);

    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    void InitDescriptors();
};