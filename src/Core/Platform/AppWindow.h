#pragma once

#include <expected>
#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace Tumbler {

enum class WindowError {
    GLFWInitFailed,
    WindowCreationFailed,
    SurfaceCreationFailed,
};

class AppWindow {
public:
    std::expected<void, WindowError> Init(int width, int height, const char* title);
    void Shutdown();

    bool ShouldClose() const;
    void PollEvents() const;

    std::expected<VkSurfaceKHR, WindowError> CreateSurface(VkInstance instance) const;
    void GetFramebufferSize(int* w, int* h) const;

    GLFWwindow* GetHandle() const { return m_Window; }

private:
    GLFWwindow* m_Window = nullptr;
};

} // namespace Tumbler
