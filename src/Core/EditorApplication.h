#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <memory>
#include <string>

#include "UI/EditorUI.h"

namespace Tumbler {

class AppWindow;
class VulkanDevice;
class Swapchain;
class DeletionQueue;
class CommandManager;
class ImGuiLayer;
class Camera;
class ClusterRenderer;

// ────────────────────────────────────────────────────────────
// EditorApplication — owns the entire editor lifecycle.
// Create → Init → Run → Shutdown.
// ────────────────────────────────────────────────────────────
class EditorApplication {
public:
    EditorApplication();
    ~EditorApplication();

    EditorApplication(const EditorApplication&)            = delete;
    EditorApplication& operator=(const EditorApplication&) = delete;

    // Returns false on fatal error.
    bool Init(int argc, char** argv);
    void Run();
    void Shutdown();

private:
    void RenderView(VkCommandBuffer cmd,
                    EditorUI::RenderTarget& rt,
                    const Camera& camera);
    void HandleInput(const EditorUI::FrameState& frameState,
                     float dt, float wheel);
    void RecreateSwapchain();

    // ── Window ──
    std::unique_ptr<AppWindow>     m_Window;

    // ── Vulkan ──
    std::unique_ptr<VulkanDevice>  m_Device;
    VkSurfaceKHR                   m_Surface       = VK_NULL_HANDLE;
    VmaAllocator                   m_Allocator     = VK_NULL_HANDLE;
    std::unique_ptr<Swapchain>     m_Swapchain;
    std::unique_ptr<DeletionQueue> m_DeletionQueue;
    std::unique_ptr<CommandManager> m_CmdManager;
    VkSampler                      m_RTSampler     = VK_NULL_HANDLE;
    VkFence                        m_AcquireFence  = VK_NULL_HANDLE;

    // ── UI ──
    std::unique_ptr<ImGuiLayer>    m_ImGui;
    std::unique_ptr<EditorUI>      m_EditorUI;

    // ── Rendering ──
    std::unique_ptr<ClusterRenderer> m_ClusterRenderer;
    std::unique_ptr<Camera>          m_EditorCam;
    std::unique_ptr<Camera>          m_GameCam;

    // ── Mesh path ──
    std::string m_MeshPath;
};

} // namespace Tumbler
