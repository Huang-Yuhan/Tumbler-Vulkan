#include "Core/Platform/AppWindow.h"
#include "Core/Utils/Log.h"

int main() {
    // 【变化】通过 Get() 获取单例实例，然后调用 Init
    Log::Get().Init();

    try {
        AppWindow::AppWindowConfig config;
        AppWindow window(config);

        while (!window.ShouldClose()) {
            window.PollEvents();
        }

    } catch (const std::exception& e) {
        LOG_CRITICAL("Crash: {}", e.what());
        return -1;
    }

    return 0;
}