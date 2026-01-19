#pragma once

#include "VulkanContext.h"
#include "VulkanSwapchain.h"
#include "FVulkanMesh.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>

class AppWindow;
class FMesh; // 前置声明

struct MeshPushConstants {
    glm::vec4 data;
    glm::mat4 render_matrix;
};

class VulkanRenderer {
public:
    VulkanRenderer() = default;
    ~VulkanRenderer() { Cleanup(); }

    void Init(AppWindow* window);
    void Cleanup();

    // 修改：允许传入 Mesh 指针进行绘制
    void Render(FVulkanMesh* mesh, const glm::mat4& transformMatrix);
    FVulkanMesh& UploadMesh(FMesh* cpuMesh);

    VkDevice GetDevice() const { return Context.GetDevice(); }

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
};