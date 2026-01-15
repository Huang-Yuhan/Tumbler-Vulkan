#pragma once

#include <string>
#include <cstdint>

// 必须先定义这个宏，再包含 GLFW
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class AppWindow
{
public:
    struct AppWindowConfig
    {
        std::string Title = "Tumbler Engine";
        uint32_t Width = 1280;
        uint32_t Height = 720;
        bool Resizable = true;
    };

    explicit AppWindow(const AppWindowConfig& config);
    ~AppWindow();

    //禁止拷贝
    AppWindow(const AppWindow&) = delete;
    AppWindow& operator=(const AppWindow&) = delete;

    // ==========================================
    // 核心流程
    // ==========================================

    // 处理系统事件 (鼠标、键盘、窗口消息)
    void PollEvents();

    // 检查窗口是否收到关闭信号
    [[nodiscard]] bool ShouldClose() const;

    // ==========================================
    // 功能接口
    // ==========================================

    // 获取原生 GLFW 指针 (后续 ImGui 或 Surface 创建需要)
    [[nodiscard]] GLFWwindow* GetNativeWindow() const { return Handle; }

    // 获取当前帧缓冲大小 (单位：像素，用于设置 Viewport)
    // 注意：在高 DPI 屏幕上，这可能大于 Config.Width/Height
    void GetFramebufferSize(int& width, int& height) const;

    // 创建 Vulkan Surface (连接窗口和 Vulkan Instance 的桥梁)
    VkSurfaceKHR CreateSurface(VkInstance instance);

private:
    GLFWwindow* Handle = nullptr;
    AppWindowConfig WindowConfig;

    void Init();
    void Shutdown();
};