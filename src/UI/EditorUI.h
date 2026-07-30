#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace Tumbler {

// ────────────────────────────────────────────────────────────
// EditorUI — Unity-style DockSpace editor interface
//
// Owns two offscreen render targets (Scene View, Game View).
// Draws all ImGui panels. Does NOT own cameras or rendering —
// the caller renders into the RTs before calling Draw().
// ────────────────────────────────────────────────────────────
class EditorUI {
public:
    EditorUI() = default;
    ~EditorUI() = default;

    EditorUI(const EditorUI&)            = delete;
    EditorUI& operator=(const EditorUI&) = delete;

    struct RenderTarget {
        VkImage         colorImage   = VK_NULL_HANDLE;
        VkImageView     colorView    = VK_NULL_HANDLE;
        VmaAllocation   colorAlloc   = VK_NULL_HANDLE;
        VkImage         depthImage   = VK_NULL_HANDLE;
        VkImageView     depthView    = VK_NULL_HANDLE;
        VmaAllocation   depthAlloc   = VK_NULL_HANDLE;
        VkExtent2D      extent{};
        VkDescriptorSet imguiDescSet = VK_NULL_HANDLE;  // for ImGui::Image()
    };

    // Per-frame state written by Draw(), read by caller for camera control.
    struct FrameState {
        bool sceneViewHovered  = false;
        bool sceneViewFocused  = false;
        bool gameViewHovered   = false;
        bool gameViewFocused   = false;
    };

    // ── Lifecycle ──
    bool Init(VkDevice device, VmaAllocator allocator,
              VkExtent2D initialExtent, VkFormat colorFormat,
              VkSampler sampler);
    void Shutdown();

    void OnResize(VkExtent2D newExtent);

    // ── Per-frame (call between ImGui::NewFrame and ImGui::Render) ──
    FrameState Draw(float fps, int clusterCount, int triangleCount);

    // ── Accessors ──
    RenderTarget& GetSceneViewRT() { return m_SceneViewRT; }
    RenderTarget& GetGameViewRT()  { return m_GameViewRT; }

    // ── Logging (for Console panel) ──
    void AddLog(const char* fmt, ...);

private:
    void CreateRT(RenderTarget& rt, VkExtent2D extent);
    void DestroyRT(RenderTarget& rt);
    void RegisterWithImGui(RenderTarget& rt);

    void DrawMenuBar();
    void DrawViewPanel(const char* title, RenderTarget& rt,
                       bool& outHovered, bool& outFocused);
    void DrawHierarchy();
    void DrawInspector();
    void DrawConsole();
    void DrawStatsPanel(float fps, int clusterCount, int triangleCount);

    VkDevice       m_Device       = VK_NULL_HANDLE;
    VmaAllocator   m_Allocator    = VK_NULL_HANDLE;
    VkSampler      m_Sampler      = VK_NULL_HANDLE;
    VkFormat       m_ColorFormat  = VK_FORMAT_UNDEFINED;
    VkExtent2D     m_ViewExtent{};

    RenderTarget   m_SceneViewRT;
    RenderTarget   m_GameViewRT;

    std::vector<std::string> m_LogLines;
    int  m_SelectedCluster  = -1;
    bool m_DockLayoutSet    = false;
};

} // namespace Tumbler
