#pragma once

#include "Core/Engine/EngineConfig.h"

#include <memory>

namespace Tumbler {

class AppWindow;
class AssetDatabase;
class VulkanContext;
class RenderDevice;
class CommandManager;
class VulkanSwapchain;
class DescriptorManager;
class ResourceManager;

// ============================================================================
// Engine — 生命周期编排 + 主循环驱动
// ============================================================================
// 职责: 按依赖顺序创建/销毁所有子系统，提供依赖注入，驱动主循环。
// 不设单例，由 main() 创建在栈上。
class Engine {
public:
    Engine();
    ~Engine();

    // 不可拷贝
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // 初始化所有子系统
    bool Init(const EngineConfig& config);

    // 关闭所有子系统（反向销毁）
    void Shutdown();

    // 主循环（目前仅事件轮询 + clear + present）
    void Run();

    // ---- 子系统访问器 (供后续扩展使用) ----

    AppWindow* GetWindow() const { return m_Window.get(); }
    AssetDatabase* GetAssetDatabase() const { return m_AssetDatabase.get(); }
    VulkanContext* GetVulkanContext() const { return m_VulkanContext.get(); }
    RenderDevice* GetRenderDevice() const { return m_RenderDevice.get(); }
    CommandManager* GetCommandManager() const { return m_CommandManager.get(); }
    VulkanSwapchain* GetSwapchain() const { return m_Swapchain.get(); }
    DescriptorManager* GetDescriptorManager() const { return m_DescriptorManager.get(); }
    ResourceManager* GetResourceManager() const { return m_ResourceManager.get(); }

private:
    EngineConfig m_Config;

    // 子系统所有权（unique_ptr，按创建顺序声明）
    std::unique_ptr<AssetDatabase> m_AssetDatabase;
    std::unique_ptr<AppWindow> m_Window;
    std::unique_ptr<VulkanContext> m_VulkanContext;
    std::unique_ptr<RenderDevice> m_RenderDevice;
    std::unique_ptr<CommandManager> m_CommandManager;
    std::unique_ptr<VulkanSwapchain> m_Swapchain;
    std::unique_ptr<DescriptorManager> m_DescriptorManager;
    std::unique_ptr<ResourceManager> m_ResourceManager;

    bool m_bInitialized = false;
    bool m_bRunning = false;
};

} // namespace Tumbler
