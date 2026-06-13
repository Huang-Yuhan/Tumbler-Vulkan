// AppWindow.h — GLFW 窗口 + Vulkan Surface 创建
//
// 职责: 管理 GLFW 窗口生命周期，创建 Vulkan Surface，
//       提供输入轮询和窗口状态查询。
//
// 依赖: GLFW, Vulkan
// 层级: 平台层 (Phase 1)，无项目内部依赖

#pragma once

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace Tumbler {

class GpuAppWindow {
public:
    struct Config {
        int Width = 1280;
        int Height = 720;
        const char* Title = "Tumbler";
        bool Visible = true;
        bool Resizable = true;
    };

    bool Init(const Config& cfg);
    void Shutdown();

    bool ShouldClose() const;
    void PollEvents();

    VkSurfaceKHR CreateSurface(VkInstance instance) const;
    void GetFramebufferSize(int* w, int* h) const;
    GLFWwindow* GetHandle() const;

    GpuAppWindow() = default;
    ~GpuAppWindow() = default;
    GpuAppWindow(const GpuAppWindow&) = delete;
    GpuAppWindow& operator=(const GpuAppWindow&) = delete;

private:
    GLFWwindow* m_Window = nullptr;
};

} // namespace Tumbler
