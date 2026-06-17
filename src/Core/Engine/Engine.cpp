#include "Engine.h"

#include "Core/Assets/AssetDatabase.h"
#include "Core/Graphics/CommandManager.h"
#include "Core/Graphics/DescriptorManager.h"
#include "Core/Graphics/RenderDevice.h"
#include "Core/Graphics/ResourceManager.h"
#include "Core/Graphics/VulkanContext.h"
#include "Core/Graphics/VulkanSwapchain.h"
#include "Core/Platform/AppWindow.h"

#include <iostream>

namespace Tumbler {

Engine::Engine() = default;
Engine::~Engine() = default;

bool Engine::Init(const EngineConfig& config) {
    m_Config = config;

    // ---- 1. AssetDatabase ----
    m_AssetDatabase = std::make_unique<AssetDatabase>();
    if (!m_AssetDatabase->LoadAssetMap(m_Config.AssetMapPath)) {
        std::cerr << "[Engine] Warning: Failed to load asset_map.json. Continuing without asset database." << std::endl;
    }

    // ---- 2. AppWindow ----
    AppWindow::Config windowCfg;
    windowCfg.Width = m_Config.WindowWidth;
    windowCfg.Height = m_Config.WindowHeight;
    windowCfg.Title = m_Config.WindowTitle.c_str();

    m_Window = std::make_unique<AppWindow>();
    if (!m_Window->Init(windowCfg)) {
        std::cerr << "[Engine] Failed to create window." << std::endl;
        return false;
    }

    // ---- 3. VulkanContext ----
    m_VulkanContext = std::make_unique<VulkanContext>();
    if (!m_VulkanContext->Init(m_Window.get())) {
        std::cerr << "[Engine] Failed to initialize Vulkan context." << std::endl;
        return false;
    }

    // ---- 4. RenderDevice ----
    m_RenderDevice = std::make_unique<RenderDevice>();
    if (!m_RenderDevice->Init(m_VulkanContext->GetInstance(), m_VulkanContext->GetDevice(),
                              m_VulkanContext->GetPhysicalDevice())) {
        std::cerr << "[Engine] Failed to initialize render device." << std::endl;
        return false;
    }

    // ---- 5. CommandManager ----
    m_CommandManager = std::make_unique<CommandManager>();
    if (!m_CommandManager->Init(m_VulkanContext->GetDevice(), m_VulkanContext->GetGraphicsQueueFamily())) {
        std::cerr << "[Engine] Failed to initialize command manager." << std::endl;
        return false;
    }

    // ---- 6. VulkanSwapchain ----
    m_Swapchain = std::make_unique<VulkanSwapchain>();
    if (!m_Swapchain->Init(m_VulkanContext->GetInstance(), m_VulkanContext->GetPhysicalDevice(),
                           m_VulkanContext->GetDevice(), m_VulkanContext->GetSurface(),
                           m_VulkanContext->GetGraphicsQueueFamily(), m_VulkanContext->GetPresentQueueFamily(),
                           *m_RenderDevice, m_Config.WindowWidth, m_Config.WindowHeight)) {
        std::cerr << "[Engine] Failed to initialize swapchain." << std::endl;
        return false;
    }

    // ---- 7. DescriptorManager ----
    m_DescriptorManager = std::make_unique<DescriptorManager>();
    if (!m_DescriptorManager->Init(m_VulkanContext->GetDevice(), 1024)) {
        std::cerr << "[Engine] Failed to initialize descriptor manager." << std::endl;
        return false;
    }

    // ---- 8. ResourceManager ----
    m_ResourceManager = std::make_unique<ResourceManager>();
    if (!m_ResourceManager->Init(m_VulkanContext->GetDevice(), *m_RenderDevice, *m_CommandManager)) {
        std::cerr << "[Engine] Failed to initialize resource manager." << std::endl;
        return false;
    }

    m_bInitialized = true;
    std::cout << "[Engine] Initialized successfully." << std::endl;
    return true;
}

void Engine::Shutdown() {
    // 反向销毁：最后创建的先销毁
    m_ResourceManager.reset();
    m_DescriptorManager.reset();
    m_Swapchain.reset();
    m_CommandManager.reset();
    m_RenderDevice.reset();
    m_VulkanContext.reset();
    m_Window.reset();
    m_AssetDatabase.reset();

    m_bInitialized = false;
    m_bRunning = false;

    std::cout << "[Engine] Shutdown complete." << std::endl;
}

void Engine::Run() {
    if (!m_bInitialized) {
        std::cerr << "[Engine] Run() called before Init()." << std::endl;
        return;
    }

    m_bRunning = true;

    while (m_bRunning && !m_Window->ShouldClose()) {
        m_Window->PollEvents();

        if (m_Window->ShouldClose())
            break;

        // 获取交换链图像
        uint32_t imageIndex = 0;
        VkResult result = m_Swapchain->AcquireNextImage(&imageIndex, VK_NULL_HANDLE, VK_NULL_HANDLE);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            // 窗口 resize → 重建交换链
            int w, h;
            m_Window->GetFramebufferSize(&w, &h);
            if (w > 0 && h > 0) {
                m_Swapchain->Recreate(w, h);
            }
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            break;
        }

        // Present（后续加入 Renderer）
        m_Swapchain->Present(m_VulkanContext->GetGraphicsQueue(), imageIndex, VK_NULL_HANDLE);
    }

    // 等待 GPU 完成
    vkDeviceWaitIdle(m_VulkanContext->GetDevice());
    m_bRunning = false;
}

} // namespace Tumbler
