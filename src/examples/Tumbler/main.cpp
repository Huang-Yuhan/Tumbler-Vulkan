#include "Core/Platform/AppWindow.h"
#include "Core/Utils/Log.h"
#include "Core/Graphics/VulkanRenderer.h" // 只需要引用这个

int main() {
    Log::Get().Init();
    LOG_INFO("Tumbler Engine Starting...");

    try {
        // 1. 创建窗口
        AppWindow::AppWindowConfig config;
        config.Title = "Tumbler Engine v0.1 - Render Loop Running";
        config.Width = 800;
        config.Height = 600;
        AppWindow window(config);

        // 2. 创建渲染器 (它会自动搞定 Context 和 Swapchain)
        VulkanRenderer renderer;
        renderer.Init(&window);

        // 3. 主循环
        while (!window.ShouldClose()) {
            window.PollEvents();

            // 4. 每一帧调用渲染
            // (虽然现在屏幕是黑的，但 GPU 已经在疯狂运转了)
            renderer.Render(nullptr); // scene 暂时传 nullptr
        }

    } catch (const std::exception& e) {
        LOG_CRITICAL("Crash: {}", e.what());
        return -1;
    }

    return 0;
}