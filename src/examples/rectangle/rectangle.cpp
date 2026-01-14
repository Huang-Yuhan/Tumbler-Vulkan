#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <memory>
#include <cassert>

// --- 引入核心模块 ---
#include "../../core/VulkanWindow.h"
#include "../../core/VulkanContext.h"
#include "../../core/VulkanDevice.h"
#include "../../core/renderer.h"
#include "core/VulkanPipeline.h"
#include "core/VulkanModel.h"

using namespace Tumbler;

// 全局常量
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

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
    std::unique_ptr<VulkanContext> vulkanContext; // 管理 Instance & Validation Layers
    std::unique_ptr<VulkanDevice> vulkanDevice;
    std::unique_ptr<Renderer> renderer;           // 管理 Rendering Loop & SwapChain

    // --- 渲染资源 ---
    std::unique_ptr<VulkanPipeline> vulkanPipeline;
    std::unique_ptr<VulkanModel> vulkanModel;

    // --- 句柄 (App 层手动管理生命周期的对象) ---
    VkSurfaceKHR surface;
    VkPipelineLayout pipelineLayout;

    void initVulkan() {
        // 1. 创建上下文 (Instance + Debug Messenger)
        vulkanContext = std::make_unique<VulkanContext>(*vulkanWindow);

        // 2. 创建 Surface
        // 注意：Window 负责调用 GLFW，Context 提供 Instance 句柄
        vulkanWindow->createSurface(vulkanContext->getInstance(), &surface);

        // 3. 创建 Device (传入 Instance 和 Surface)
        // Device 内部会保存 Surface 的副本以查询支持情况
        vulkanDevice = std::make_unique<VulkanDevice>(vulkanContext->getInstance(), surface);

        // 4. 创建 Renderer (核心渲染器)
        // 它会自动创建 SwapChain 和 CommandBuffers
        renderer = std::make_unique<Renderer>(*vulkanWindow, *vulkanDevice);

        // 5. 加载应用层资源 (模型、管线)
        loadModels();
        createPipelineLayout();
        createPipeline();
    }

    void mainLoop() {
        while (!vulkanWindow->shouldClose()) {
            glfwPollEvents();

            // === 渲染流程 ===

            // 1. 开启新的一帧 (Renderer 内部处理 AcquireImage, Resize, ResetBuffer)
            if (auto commandBuffer = renderer->beginFrame()) {

                // 2. 开启基于 SwapChain 的 RenderPass (自动处理 Viewport/Scissor)
                renderer->beginSwapChainRenderPass(commandBuffer);

                // --- 具体的绘制命令 ---
                vulkanPipeline->bind(commandBuffer);
                vulkanModel->bind(commandBuffer);
                vulkanModel->draw(commandBuffer);
                // --------------------

                // 3. 结束 RenderPass
                renderer->endSwapChainRenderPass(commandBuffer);

                // 4. 结束并提交一帧 (Renderer 内部处理 Submit, Present)
                renderer->endFrame();
            }
        }
        vkDeviceWaitIdle(vulkanDevice->getDevice());
    }

    void loadModels() {
        std::vector<VulkanModel::Vertex> vertices = {
            {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // 红
            {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, // 绿
            {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, // 蓝
            {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}}  // 白
        };
        std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};
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
        // 确保 Renderer 已初始化，因为我们需要 RenderPass
        assert(renderer != nullptr && "Cannot create pipeline before renderer");

        PipelineConfigInfo pipelineConfig{};
        VulkanPipeline::defaultPipelineConfigInfo(pipelineConfig);

        // [关键] 从 Renderer 获取 RenderPass
        pipelineConfig.renderPass = renderer->getSwapChainRenderPass();
        pipelineConfig.pipelineLayout = pipelineLayout;

        // 设置顶点输入
        auto bindingDescriptions = VulkanModel::Vertex::getBindingDescriptions();
        auto attributeDescriptions = VulkanModel::Vertex::getAttributeDescriptions();
        pipelineConfig.vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
        pipelineConfig.vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
        pipelineConfig.vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        pipelineConfig.vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        vulkanPipeline = std::make_unique<VulkanPipeline>(
            *vulkanDevice,
            "assets/shaders/example/shader.vert.spv",
            "assets/shaders/example/shader.frag.spv",
            pipelineConfig
        );
    }

    void cleanup() {
        // 1. 等待 GPU 空闲
        vkDeviceWaitIdle(vulkanDevice->getDevice());

        // 2. 清理 Pipeline 相关资源 (Layout 是裸指针，Pipeline 是智能指针)
        vulkanPipeline.reset();
        vkDestroyPipelineLayout(vulkanDevice->getDevice(), pipelineLayout, nullptr);

        // 3. 清理 Model
        vulkanModel.reset();

        // 4. 清理 Renderer (内部清理 SwapChain, RenderPass, CommandBuffers)
        renderer.reset();

        // 5. 清理 Device
        vulkanDevice.reset();

        // 6. 清理 Surface (必须在 Instance 销毁前，Device 销毁后)
        vkDestroySurfaceKHR(vulkanContext->getInstance(), surface, nullptr);

        // 7. 清理 Context (Instance & Debug Messenger)
        vulkanContext.reset();

        // 8. 清理 Window
        vulkanWindow.reset();
    }
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