#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <array>
#include <memory> // 必须包含，用于 std::unique_ptr

// 引入你封装好的头文件
#include "core/VulkanDevice.h"

using namespace Tumbler; // 为了方便使用 VulkanDevice

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;
const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// 注意：deviceExtensions 已经在 VulkanDevice 内部被定义和启用了，
// 这里如果是为了检查 Instance 层的扩展则保留，但通常 Swapchain 是 Device 扩展。
// 为了保持 swapChainSupport 查询逻辑的兼容性，我们这里只保留名字引用，
// 但实际创建 Device 时使用的是 VulkanDevice 类内部的静态数组。
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// ... Vertex 结构体和数据保持不变 ...
struct Vertex {
    struct Vec2 { float x, y; };
    struct Vec3 { float r, g, b; };
    Vec2 pos;
    Vec3 color;
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{.binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
        return bindingDescription;
    }
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
        attributeDescriptions[0] = {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, pos)};
        attributeDescriptions[1] = {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, color)};
        return attributeDescriptions;
    }
};

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}}
};
const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

// ... Debug Messenger 代理函数保持不变 ...
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) return func(instance, pCreateInfo, pAllocator, pDebugMessenger); else return VK_ERROR_EXTENSION_NOT_PRESENT;
}
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) func(instance, debugMessenger, pAllocator);
}

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class HelloRectangleApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;

    // [修改点 1] 使用智能指针管理 VulkanDevice
    // 删除了原来的 physicalDevice, device, graphicsQueue, presentQueue, commandPool
    std::unique_ptr<VulkanDevice> vulkanDevice;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    // commandPool 已移入 vulkanDevice
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Rectangle", nullptr, nullptr);
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();

        // [修改点 2] 初始化 VulkanDevice (它会自动 Pick Physical Device, Create Logical Device, Create Command Pool)
        // 这一行代码替代了原来的 pickPhysicalDevice(), createLogicalDevice(), createCommandPool()
        vulkanDevice = std::make_unique<VulkanDevice>(instance, surface);

        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        // createCommandPool(); // 已移除，由 VulkanDevice 构造函数处理
        createVertexBuffer();
        createIndexBuffer();
        createCommandBuffers();
        createSyncObjects();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }
        // [修改点 3] 获取 device 句柄
        vkDeviceWaitIdle(vulkanDevice->getDevice());
    }

    void cleanup() {
        VkDevice device = vulkanDevice->getDevice(); // 临时获取句柄方便调用

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }

        // vkDestroyCommandPool... [移除] commandPool 会在 VulkanDevice 析构时自动销毁

        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);

        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }

        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);

        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(device, swapChain, nullptr);

        // [修改点 4] 销毁 Device (通过重置智能指针触发析构函数)
        // 注意：必须在销毁 Surface 和 Instance 之前销毁 Device
        vulkanDevice.reset();

        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE};

        // [修改点 5] 使用 vulkanDevice->getDevice()
        if (vkCreateBuffer(vulkanDevice->getDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(vulkanDevice->getDevice(), buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memRequirements.size,
            // [修改点 6] 使用 vulkanDevice->findMemoryType
            .memoryTypeIndex = vulkanDevice->findMemoryType(memRequirements.memoryTypeBits, properties)
        };

        if (vkAllocateMemory(vulkanDevice->getDevice(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        vkBindBufferMemory(vulkanDevice->getDevice(), buffer, bufferMemory, 0);
    }

    void createVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
        createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBuffer, vertexBufferMemory);

        void* data;
        // [修改点 7] getDevice()
        vkMapMemory(vulkanDevice->getDevice(), vertexBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, vertices.data(), (size_t) bufferSize);
        vkUnmapMemory(vulkanDevice->getDevice(), vertexBufferMemory);
    }

    void createIndexBuffer() {
        VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
        createBuffer(bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexBuffer, indexBufferMemory);

        void* data;
        vkMapMemory(vulkanDevice->getDevice(), indexBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, indices.data(), (size_t) bufferSize);
        vkUnmapMemory(vulkanDevice->getDevice(), indexBufferMemory);
    }

    // [移除] findMemoryType 被移除，直接使用 vulkanDevice->findMemoryType

    void createGraphicsPipeline() {
        auto vertShaderCode = readFile("shaders/example/shader.vert.spv");
        auto fragShaderCode = readFile("shaders/example/shader.frag.spv");

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        // ... 省略中间配置 (Pipeline 设置代码与之前完全相同) ...
        // 只要把用到 device 的地方改成 vulkanDevice->getDevice() 即可
        // 这里为了节省篇幅，省略部分未变动的结构体初始化代码

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vertShaderModule, .pName = "main"};
        VkPipelineShaderStageCreateInfo fragShaderStageInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fragShaderModule, .pName = "main"};
        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &bindingDescription, .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()), .pVertexAttributeDescriptions = attributeDescriptions.data()};
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, .primitiveRestartEnable = VK_FALSE};
        VkPipelineViewportStateCreateInfo viewportState{.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
        VkPipelineRasterizationStateCreateInfo rasterizer{.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .depthClampEnable = VK_FALSE, .rasterizerDiscardEnable = VK_FALSE, .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_BACK_BIT, .frontFace = VK_FRONT_FACE_CLOCKWISE, .depthBiasEnable = VK_FALSE, .lineWidth = 1.0f};
        VkPipelineMultisampleStateCreateInfo multisampling{.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT, .sampleShadingEnable = VK_FALSE};
        VkPipelineColorBlendAttachmentState colorBlendAttachment{.blendEnable = VK_FALSE, .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
        VkPipelineColorBlendStateCreateInfo colorBlending{.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .logicOpEnable = VK_FALSE, .attachmentCount = 1, .pAttachments = &colorBlendAttachment};
        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()), .pDynamicStates = dynamicStates.data()};
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 0, .pushConstantRangeCount = 0};

        if (vkCreatePipelineLayout(vulkanDevice->getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = shaderStages,
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            // ... 其他状态 ...
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = nullptr,
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamicState,
            .layout = pipelineLayout,
            .renderPass = renderPass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE
        };

        if (vkCreateGraphicsPipelines(vulkanDevice->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        // Shader Module 临时用一下就可以销毁了
        vkDestroyShaderModule(vulkanDevice->getDevice(), fragShaderModule, nullptr);
        vkDestroyShaderModule(vulkanDevice->getDevice(), vertShaderModule, nullptr);
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        // ... (recordCommandBuffer 逻辑完全不变，因为它不依赖 device 句柄，只依赖 commandBuffer) ...
        VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) throw std::runtime_error("failed to begin recording command buffer!");
        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        VkRenderPassBeginInfo renderPassInfo{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, .renderPass = renderPass, .framebuffer = swapChainFramebuffers[imageIndex], .renderArea = {.offset = {0, 0}, .extent = swapChainExtent}, .clearValueCount = 1, .pClearValues = &clearColor};
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        VkViewport viewport{.x = 0.0f, .y = 0.0f, .width = (float) swapChainExtent.width, .height = (float) swapChainExtent.height, .minDepth = 0.0f, .maxDepth = 1.0f};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        VkRect2D scissor{.offset = {0, 0}, .extent = swapChainExtent};
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
        vkCmdEndRenderPass(commandBuffer);
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) throw std::runtime_error("failed to record command buffer!");
    }

    void drawFrame() {
        // [修改点 8] getDevice(), getGraphicsQueue(), getPresentQueue()
        vkWaitForFences(vulkanDevice->getDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vulkanDevice->getDevice(), swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        vkResetFences(vulkanDevice->getDevice(), 1, &inFlightFences[currentFrame]);
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};

        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = waitSemaphores,
            .pWaitDstStageMask = waitStages,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffers[currentFrame],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = signalSemaphores
        };

        if (vkQueueSubmit(vulkanDevice->getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = signalSemaphores,
            .swapchainCount = 1,
            .pSwapchains = &swapChain,
            .pImageIndices = &imageIndex
        };

        vkQueuePresentKHR(vulkanDevice->getPresentQueue(), &presentInfo);
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    // createInstance, setupDebugMessenger, createSurface ... 保持不变 ...
    void createInstance() { /* ... 代码同上 ... */
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
    void createSurface() { if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) throw std::runtime_error("failed to create window surface!"); }

    // [移除] pickPhysicalDevice() 被移除
    // [移除] createLogicalDevice() 被移除
    // [移除] findQueueFamilies(device) 被移除 (我们直接用 vulkanDevice->getQueueFamilies())

    void createSwapChain() {
        // [修改点 9] 使用 vulkanDevice->getPhysicalDevice()
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(vulkanDevice->getPhysicalDevice());

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) imageCount = swapChainSupport.capabilities.maxImageCount;
        VkSwapchainCreateInfoKHR createInfo{.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .surface = surface, .minImageCount = imageCount, .imageFormat = surfaceFormat.format, .imageColorSpace = surfaceFormat.colorSpace, .imageExtent = extent, .imageArrayLayers = 1, .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};

        // [修改点 10] 使用 vulkanDevice->getQueueFamilies()
        QueueFamilyIndices indices = vulkanDevice->getQueueFamilies();

        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
        if (indices.graphicsFamily != indices.presentFamily) { createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; createInfo.queueFamilyIndexCount = 2; createInfo.pQueueFamilyIndices = queueFamilyIndices; } else { createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; }
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform; createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; createInfo.presentMode = presentMode; createInfo.clipped = VK_TRUE; createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(vulkanDevice->getDevice(), &createInfo, nullptr, &swapChain) != VK_SUCCESS) throw std::runtime_error("failed to create swap chain!");
        vkGetSwapchainImagesKHR(vulkanDevice->getDevice(), swapChain, &imageCount, nullptr); swapChainImages.resize(imageCount); vkGetSwapchainImagesKHR(vulkanDevice->getDevice(), swapChain, &imageCount, swapChainImages.data());
        swapChainImageFormat = surfaceFormat.format; swapChainExtent = extent;
    }

    void createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = swapChainImages[i], .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = swapChainImageFormat, .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY, .g = VK_COMPONENT_SWIZZLE_IDENTITY, .b = VK_COMPONENT_SWIZZLE_IDENTITY, .a = VK_COMPONENT_SWIZZLE_IDENTITY}, .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};
            if (vkCreateImageView(vulkanDevice->getDevice(), &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) throw std::runtime_error("failed to create image views!");
        }
    }

    void createRenderPass() {
        // ... (renderPass 创建逻辑不变，除了 device 句柄) ...
        VkAttachmentDescription colorAttachment{.format = swapChainImageFormat, .samples = VK_SAMPLE_COUNT_1_BIT, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE, .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
        VkAttachmentReference colorAttachmentRef{.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount = 1, .pColorAttachments = &colorAttachmentRef};
        VkSubpassDependency dependency{.srcSubpass = VK_SUBPASS_EXTERNAL, .dstSubpass = 0, .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, .srcAccessMask = 0, .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};
        VkRenderPassCreateInfo renderPassInfo{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .attachmentCount = 1, .pAttachments = &colorAttachment, .subpassCount = 1, .pSubpasses = &subpass, .dependencyCount = 1, .pDependencies = &dependency};
        if (vkCreateRenderPass(vulkanDevice->getDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) throw std::runtime_error("failed to create render pass!");
    }

    void createFramebuffers() {
        swapChainFramebuffers.resize(swapChainImageViews.size());
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            VkImageView attachments[] = {swapChainImageViews[i]};
            VkFramebufferCreateInfo framebufferInfo{.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, .renderPass = renderPass, .attachmentCount = 1, .pAttachments = attachments, .width = swapChainExtent.width, .height = swapChainExtent.height, .layers = 1};
            if (vkCreateFramebuffer(vulkanDevice->getDevice(), &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) throw std::runtime_error("failed to create framebuffer!");
        }
    }

    // [移除] createCommandPool() 被移除

    void createCommandBuffers() {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            // [修改点 11] 使用 vulkanDevice->getCommandPool()
            .commandPool = vulkanDevice->getCommandPool(),
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = (uint32_t) commandBuffers.size()
        };

        if (vkAllocateCommandBuffers(vulkanDevice->getDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT); renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT); inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(vulkanDevice->getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(vulkanDevice->getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(vulkanDevice->getDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
                throw std::runtime_error("failed to create synchronization objects!");
        }
    }

    // ... 下面的辅助函数基本不变，只在 readFile 和 shader module 创建处需要注意 ...
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) { createInfo = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT, .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, .pfnUserCallback = debugCallback}; }

    // [注意] SwapChainSupportDetails 逻辑保留，因为 VulkanDevice 类没有包含这一块
    // 但查询时需要传入 vulkanDevice->getPhysicalDevice()
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) { SwapChainSupportDetails details; vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities); uint32_t formatCount; vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr); if (formatCount != 0) { details.formats.resize(formatCount); vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data()); } uint32_t presentModeCount; vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr); if (presentModeCount != 0) { details.presentModes.resize(presentModeCount); vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data()); } return details; }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) { for (const auto& availableFormat : availableFormats) { if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return availableFormat; } return availableFormats[0]; }
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) { for (const auto& availablePresentMode : availablePresentModes) { if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) return availablePresentMode; } return VK_PRESENT_MODE_FIFO_KHR; }
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) { if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) { return capabilities.currentExtent; } else { int width, height; glfwGetFramebufferSize(window, &width, &height); VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width); actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height); return actualExtent; } }

    static std::vector<char> readFile(const std::string& filename) { std::ifstream file(filename, std::ios::ate | std::ios::binary); if (!file.is_open()) throw std::runtime_error("failed to open file: " + filename); size_t fileSize = (size_t) file.tellg(); std::vector<char> buffer(fileSize); file.seekg(0); file.read(buffer.data(), fileSize); file.close(); return buffer; }

    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = code.size(), .pCode = reinterpret_cast<const uint32_t*>(code.data())};
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(vulkanDevice->getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) throw std::runtime_error("failed to create shader module!");
        return shaderModule;
    }

    std::vector<const char*> getRequiredExtensions() { uint32_t glfwExtensionCount = 0; const char** glfwExtensions; glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount); std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount); if (enableValidationLayers) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); return extensions; }
    bool checkValidationLayerSupport() { uint32_t layerCount; vkEnumerateInstanceLayerProperties(&layerCount, nullptr); std::vector<VkLayerProperties> availableLayers(layerCount); vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()); for (const char* layerName : validationLayers) { bool layerFound = false; for (const auto& layerProperties : availableLayers) { if (strcmp(layerName, layerProperties.layerName) == 0) { layerFound = true; break; } } if (!layerFound) return false; } return true; }
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