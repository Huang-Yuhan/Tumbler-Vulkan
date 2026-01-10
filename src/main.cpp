#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// 引入 GLM (虽然 VulkanModel.h 引用了，但在 main 中显式定义宏是一个好习惯)
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <array>
#include <memory>
#include <algorithm>

// --- 引入封装好的核心模块 ---
#include "core/VulkanDevice.h"
#include "core/VulkanWindow.h"
#include "core/VulkanPipeline.h"
#include "core/VulkanModel.h" // 新增：模型管理

using namespace Tumbler;

// 全局常量
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;
const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

// --- 辅助函数：Debug Messenger ---
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) return func(instance, pCreateInfo, pAllocator, pDebugMessenger); else return VK_ERROR_EXTENSION_NOT_PRESENT;
}
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) func(instance, debugMessenger, pAllocator);
}

// --- 辅助结构体：Swap Chain 支持信息 (尚未封装) ---
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class HelloRectangleApplication {
public:
    void run() {
        // 1. 初始化窗口
        vulkanWindow = std::make_unique<VulkanWindow>(WIDTH, HEIGHT, "Tumbler Engine");

        // 2. 初始化 Vulkan 核心
        initVulkan();

        // 3. 进入主循环
        mainLoop();

        // 4. 清理资源
        cleanup();
    }

private:
    // --- 核心组件智能指针 ---
    std::unique_ptr<VulkanWindow> vulkanWindow;
    std::unique_ptr<VulkanDevice> vulkanDevice;
    std::unique_ptr<VulkanPipeline> vulkanPipeline;
    std::unique_ptr<VulkanModel> vulkanModel; // 模型对象

    // --- Vulkan 句柄 ---
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;

    std::vector<VkCommandBuffer> commandBuffers;

    // --- 同步对象 ---
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();

        // 初始化 Device
        vulkanDevice = std::make_unique<VulkanDevice>(instance, surface);

        createSwapChain();
        createImageViews();
        createRenderPass();

        // 创建 Pipeline 布局 (DescriptorSets, PushConstants 在这里定义)
        createPipelineLayout();

        // 创建 Pipeline (依赖 RenderPass 和 Layout)
        createPipeline();

        // 加载模型数据 (顶点/索引)
        loadModels();

        createFramebuffers();

        createCommandBuffers();
        createSyncObjects();
    }

    void mainLoop() {
        while (!vulkanWindow->shouldClose()) {
            glfwPollEvents();
            drawFrame();
        }
        vkDeviceWaitIdle(vulkanDevice->getDevice());
    }

    // --- 加载模型数据 ---
    void loadModels() {
        // 使用 VulkanModel::Vertex 定义数据
        std::vector<VulkanModel::Vertex> vertices = {
            // position (vec2)         // color (vec3)
            {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // 红
            {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, // 绿
            {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, // 蓝
            {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}}  // 白
        };

        std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

        // 创建模型实例 (会自动创建 Vertex Buffer 和 Index Buffer)
        vulkanModel = std::make_unique<VulkanModel>(*vulkanDevice, vertices, indices);
    }

    void createPipelineLayout() {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(vulkanDevice->getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }
    }

    void createPipeline() {
        // 1. 获取默认配置
        PipelineConfigInfo pipelineConfig{};
        VulkanPipeline::defaultPipelineConfigInfo(pipelineConfig);

        // 2. 填充上下文相关配置
        pipelineConfig.renderPass = renderPass;
        pipelineConfig.pipelineLayout = pipelineLayout;

        // 3. 从 VulkanModel 获取顶点输入描述 (关键步骤)
        auto bindingDescriptions = VulkanModel::Vertex::getBindingDescriptions();
        auto attributeDescriptions = VulkanModel::Vertex::getAttributeDescriptions();

        pipelineConfig.vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
        pipelineConfig.vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
        pipelineConfig.vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        pipelineConfig.vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        // 4. 创建管线
        vulkanPipeline = std::make_unique<VulkanPipeline>(
            *vulkanDevice,
            "shaders/example/shader.vert.spv",
            "shaders/example/shader.frag.spv",
            pipelineConfig
        );
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // 1. 绑定管线
        vulkanPipeline->bind(commandBuffer);

        // ================= [修复开始] =================
        // 因为开启了动态状态，必须在绘制前手动设置 Viewport 和 Scissor
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapChainExtent.width);
        viewport.height = static_cast<float>(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        // ================= [修复结束] =================

        // 2. 绑定模型数据并绘制
        vulkanModel->bind(commandBuffer);
        vulkanModel->draw(commandBuffer);

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void drawFrame() {
        vkWaitForFences(vulkanDevice->getDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vulkanDevice->getDevice(), swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            // recreateSwapChain(); // 暂时忽略
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        vkResetFences(vulkanDevice->getDevice(), 1, &inFlightFences[currentFrame]);

        vkResetCommandBuffer(commandBuffers[currentFrame], 0);
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(vulkanDevice->getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapChain;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(vulkanDevice->getPresentQueue(), &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || vulkanWindow->wasWindowResized()) {
            vulkanWindow->resetWindowResizedFlag();
            // recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void cleanup() {
        VkDevice device = vulkanDevice->getDevice();
        vkDeviceWaitIdle(device);

        // 安全：根据实际大小清理，如果是空的就不会执行循环
        for (size_t i = 0; i < renderFinishedSemaphores.size(); i++) {
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        }
        for (size_t i = 0; i < imageAvailableSemaphores.size(); i++) {
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        }
        for (size_t i = 0; i < inFlightFences.size(); i++) {
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }

        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }

        // 清理封装对象 (注意顺序)
        vulkanModel.reset();      // 先清理模型 (Buffer)
        vulkanPipeline.reset();   // 清理管线

        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);

        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(device, swapChain, nullptr);

        vulkanDevice.reset(); // 清理设备

        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        vulkanWindow.reset(); // 最后清理窗口
    }

    // --- 以下是未封装的辅助函数 (SwapChain 创建等) ---
    // 为了节省篇幅，这些函数逻辑与之前完全一致，仅展示函数名
    // 你需要确保它们在你的文件中存在

    void createInstance() { /* ... 代码同前 ... */
        if (enableValidationLayers && !checkValidationLayerSupport()) throw std::runtime_error("validation layers requested, but not available!");
        VkApplicationInfo appInfo{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "Hello Rectangle", .applicationVersion = VK_MAKE_VERSION(1, 0, 0), .pEngineName = "No Engine", .engineVersion = VK_MAKE_VERSION(1, 0, 0), .apiVersion = VK_API_VERSION_1_0};
        VkInstanceCreateInfo createInfo{.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &appInfo};
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) { createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size()); createInfo.ppEnabledLayerNames = validationLayers.data(); populateDebugMessengerCreateInfo(debugCreateInfo); createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo; } else { createInfo.enabledLayerCount = 0; createInfo.pNext = nullptr; }
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) throw std::runtime_error("failed to create instance!");
    }

    void setupDebugMessenger() { if (!enableValidationLayers) return; VkDebugUtilsMessengerCreateInfoEXT createInfo; populateDebugMessengerCreateInfo(createInfo); if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) throw std::runtime_error("failed to set up debug messenger!"); }
    void createSurface() { vulkanWindow->createSurface(instance, &surface); }

    void createSwapChain() { /* ... 使用 vulkanDevice 和 vulkanWindow 获取信息的逻辑 ... */
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(vulkanDevice->getPhysicalDevice());
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) imageCount = swapChainSupport.capabilities.maxImageCount;
        VkSwapchainCreateInfoKHR createInfo{.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .surface = surface, .minImageCount = imageCount, .imageFormat = surfaceFormat.format, .imageColorSpace = surfaceFormat.colorSpace, .imageExtent = extent, .imageArrayLayers = 1, .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
        QueueFamilyIndices indices = vulkanDevice->getQueueFamilies();
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
        if (indices.graphicsFamily != indices.presentFamily) { createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; createInfo.queueFamilyIndexCount = 2; createInfo.pQueueFamilyIndices = queueFamilyIndices; } else { createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; }
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform; createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; createInfo.presentMode = presentMode; createInfo.clipped = VK_TRUE; createInfo.oldSwapchain = VK_NULL_HANDLE;
        if (vkCreateSwapchainKHR(vulkanDevice->getDevice(), &createInfo, nullptr, &swapChain) != VK_SUCCESS) throw std::runtime_error("failed to create swap chain!");
        vkGetSwapchainImagesKHR(vulkanDevice->getDevice(), swapChain, &imageCount, nullptr); swapChainImages.resize(imageCount); vkGetSwapchainImagesKHR(vulkanDevice->getDevice(), swapChain, &imageCount, swapChainImages.data());
        swapChainImageFormat = surfaceFormat.format; swapChainExtent = extent;
    }

    void createImageViews() { /* ... 代码同前 ... */
        swapChainImageViews.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = swapChainImages[i], .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = swapChainImageFormat, .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY}, .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};
            if (vkCreateImageView(vulkanDevice->getDevice(), &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) throw std::runtime_error("failed to create image views!");
        }
    }

    void createRenderPass() { /* ... 代码同前 ... */
        VkAttachmentDescription colorAttachment{.format = swapChainImageFormat, .samples = VK_SAMPLE_COUNT_1_BIT, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE, .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
        VkAttachmentReference colorAttachmentRef{.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount = 1, .pColorAttachments = &colorAttachmentRef};
        VkSubpassDependency dependency{.srcSubpass = VK_SUBPASS_EXTERNAL, .dstSubpass = 0, .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, .srcAccessMask = 0, .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};
        VkRenderPassCreateInfo renderPassInfo{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .attachmentCount = 1, .pAttachments = &colorAttachment, .subpassCount = 1, .pSubpasses = &subpass, .dependencyCount = 1, .pDependencies = &dependency};
        if (vkCreateRenderPass(vulkanDevice->getDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) throw std::runtime_error("failed to create render pass!");
    }

    void createFramebuffers() { /* ... 代码同前 ... */
        swapChainFramebuffers.resize(swapChainImageViews.size());
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            VkImageView attachments[] = {swapChainImageViews[i]};
            VkFramebufferCreateInfo framebufferInfo{.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, .renderPass = renderPass, .attachmentCount = 1, .pAttachments = attachments, .width = swapChainExtent.width, .height = swapChainExtent.height, .layers = 1};
            if (vkCreateFramebuffer(vulkanDevice->getDevice(), &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) throw std::runtime_error("failed to create framebuffer!");
        }
    }

    void createCommandBuffers() {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = vulkanDevice->getCommandPool(), .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = (uint32_t) commandBuffers.size()};
        if (vkAllocateCommandBuffers(vulkanDevice->getDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) throw std::runtime_error("failed to allocate command buffers!");
    }

    void createSyncObjects() { /* ... 代码同前 ... */
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT); renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT); inFlightFences.resize(MAX_FRAMES_IN_FLIGHT); VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}; VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT}; for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) { if (vkCreateSemaphore(vulkanDevice->getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS || vkCreateSemaphore(vulkanDevice->getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS || vkCreateFence(vulkanDevice->getDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) throw std::runtime_error("failed to create synchronization objects!"); }
    }

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) { SwapChainSupportDetails details; vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities); uint32_t formatCount; vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr); if (formatCount != 0) { details.formats.resize(formatCount); vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data()); } uint32_t presentModeCount; vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr); if (presentModeCount != 0) { details.presentModes.resize(presentModeCount); vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data()); } return details; }
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) { for (const auto& availableFormat : availableFormats) { if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return availableFormat; } return availableFormats[0]; }
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) { for (const auto& availablePresentMode : availablePresentModes) { if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) return availablePresentMode; } return VK_PRESENT_MODE_FIFO_KHR; }
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) { if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) { return capabilities.currentExtent; } else { VkExtent2D actualExtent = vulkanWindow->getExtent(); actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width); actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height); return actualExtent; } }
    std::vector<const char*> getRequiredExtensions() { uint32_t glfwExtensionCount = 0; const char** glfwExtensions; glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount); std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount); if (enableValidationLayers) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); return extensions; }
    bool checkValidationLayerSupport() { uint32_t layerCount; vkEnumerateInstanceLayerProperties(&layerCount, nullptr); std::vector<VkLayerProperties> availableLayers(layerCount); vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()); for (const char* layerName : validationLayers) { bool layerFound = false; for (const auto& layerProperties : availableLayers) { if (strcmp(layerName, layerProperties.layerName) == 0) { layerFound = true; break; } } if (!layerFound) return false; } return true; }
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) { createInfo = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT, .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, .pfnUserCallback = debugCallback}; }
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) { std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl; return VK_FALSE; }
};

int main() {
    HelloRectangleApplication app;
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}