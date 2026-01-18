#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <optional>

class AppWindow;

class VulkanContext
{
public:
    VulkanContext()=default;
    ~VulkanContext(){CleanUp();}

    void Init(AppWindow* window);
    void CleanUp();

    // ==========================================
    // Getter
    // ==========================================
    [[nodiscard]] VkInstance GetInstance() const { return Instance; }
    [[nodiscard]] VkDevice GetDevice() const { return Device; }
    [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const { return PhysicalDevice; }
    [[nodiscard]] VkQueue GetGraphicsQueue() const { return GraphicsQueue; }
    [[nodiscard]] uint32_t GetGraphicsQueueFamily() const { return GraphicsQueueFamilyIndex; }
    [[nodiscard]] VmaAllocator GetAllocator() const { return Allocator; }
    [[nodiscard]] VkSurfaceKHR GetSurface() const { return Surface; }


private:
    VkInstance Instance=VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT DebugMessenger=VK_NULL_HANDLE;                 //用于接受报错信息
    VkSurfaceKHR Surface=VK_NULL_HANDLE;

    VkPhysicalDevice PhysicalDevice=VK_NULL_HANDLE;
    VkDevice Device=VK_NULL_HANDLE;

    VkQueue GraphicsQueue=VK_NULL_HANDLE;
    uint32_t GraphicsQueueFamilyIndex=0;

    VmaAllocator Allocator=VK_NULL_HANDLE;

    // ==========================================
    // 内部初始化流程
    // ==========================================
    void CreateInstance();
    void SetupDebugMessenger();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void InitVMA();

    // ==========================================
    // 工具函数
    // ==========================================

    bool CheckValidationLayerSupport();
    std::vector<const char*> GetRequiredExtensions();
};