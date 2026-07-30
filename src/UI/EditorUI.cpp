#include "EditorUI.h"
#include "Core/Utils/Log.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>

#include <cstdarg>
#include <cstdio>

namespace Tumbler {

// ────────────────────────────────────────────────────────────
// Internal helpers
// ────────────────────────────────────────────────────────────

static void CreateImage(VkDevice device, VmaAllocator allocator,
                        VkExtent2D extent, VkFormat format,
                        VkImageUsageFlags usage,
                        VkImage& image, VmaAllocation& alloc) {
    VkImageCreateInfo info{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = format,
        .extent        = { extent.width, extent.height, 1 },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = usage,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo allocInfo{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };
    vmaCreateImage(allocator, &info, &allocInfo, &image, &alloc, nullptr);
}

static VkImageView CreateImageView(VkDevice device, VkImage image,
                                   VkFormat format, VkImageAspectFlags aspect) {
    VkImageViewCreateInfo info{
        .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image      = image,
        .viewType   = VK_IMAGE_VIEW_TYPE_2D,
        .format     = format,
        .subresourceRange = {
            .aspectMask     = aspect,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };
    VkImageView view;
    vkCreateImageView(device, &info, nullptr, &view);
    return view;
}

// ────────────────────────────────────────────────────────────
// Init / Shutdown
// ────────────────────────────────────────────────────────────

bool EditorUI::Init(VkDevice device, VmaAllocator allocator,
                    VkExtent2D initialExtent, VkFormat colorFormat,
                    VkSampler sampler) {
    m_Device      = device;
    m_Allocator   = allocator;
    m_Sampler     = sampler;
    m_ColorFormat = colorFormat;
    m_ViewExtent  = initialExtent;

    CreateRT(m_SceneViewRT, initialExtent);
    CreateRT(m_GameViewRT, initialExtent);

    RegisterWithImGui(m_SceneViewRT);
    RegisterWithImGui(m_GameViewRT);

    LOG_INFO("EditorUI initialized ({}x{})", initialExtent.width, initialExtent.height);
    return true;
}

void EditorUI::Shutdown() {
    DestroyRT(m_SceneViewRT);
    DestroyRT(m_GameViewRT);
    LOG_INFO("EditorUI shutdown");
}

void EditorUI::OnResize(VkExtent2D newExtent) {
    if (newExtent.width == 0 || newExtent.height == 0) return;
    if (newExtent.width == m_ViewExtent.width && newExtent.height == m_ViewExtent.height)
        return;

    // ImGui descriptor sets are stale after image recreation
    DestroyRT(m_SceneViewRT);
    DestroyRT(m_GameViewRT);

    m_ViewExtent = newExtent;
    CreateRT(m_SceneViewRT, newExtent);
    CreateRT(m_GameViewRT, newExtent);

    RegisterWithImGui(m_SceneViewRT);
    RegisterWithImGui(m_GameViewRT);

    LOG_INFO("EditorUI resized to {}x{}", newExtent.width, newExtent.height);
}

// ────────────────────────────────────────────────────────────
// RT creation / destruction
// ────────────────────────────────────────────────────────────

void EditorUI::CreateRT(RenderTarget& rt, VkExtent2D extent) {
    rt.extent = extent;

    // Color image (sampled by ImGui + rendered into as color attachment)
    CreateImage(m_Device, m_Allocator, extent, m_ColorFormat,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                rt.colorImage, rt.colorAlloc);
    rt.colorView = CreateImageView(m_Device, rt.colorImage, m_ColorFormat,
                                   VK_IMAGE_ASPECT_COLOR_BIT);

    // Depth image (D32)
    CreateImage(m_Device, m_Allocator, extent, VK_FORMAT_D32_SFLOAT,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                rt.depthImage, rt.depthAlloc);
    rt.depthView = CreateImageView(m_Device, rt.depthImage, VK_FORMAT_D32_SFLOAT,
                                   VK_IMAGE_ASPECT_DEPTH_BIT);
}

void EditorUI::DestroyRT(RenderTarget& rt) {
    if (rt.colorView)  { vkDestroyImageView(m_Device, rt.colorView, nullptr); rt.colorView = VK_NULL_HANDLE; }
    if (rt.colorImage) { vmaDestroyImage(m_Allocator, rt.colorImage, rt.colorAlloc);
                         rt.colorImage = VK_NULL_HANDLE; }
    if (rt.depthView)  { vkDestroyImageView(m_Device, rt.depthView, nullptr); rt.depthView = VK_NULL_HANDLE; }
    if (rt.depthImage) { vmaDestroyImage(m_Allocator, rt.depthImage, rt.depthAlloc);
                         rt.depthImage = VK_NULL_HANDLE; }
    rt.imguiDescSet = VK_NULL_HANDLE;
}

void EditorUI::RegisterWithImGui(RenderTarget& rt) {
    rt.imguiDescSet = ImGui_ImplVulkan_AddTexture(
        m_Sampler, rt.colorView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

// ────────────────────────────────────────────────────────────
// Main draw entry point
// ────────────────────────────────────────────────────────────

EditorUI::FrameState EditorUI::Draw(float fps, int clusterCount, int triangleCount) {
    FrameState state{};

    // ── Dockspace (full viewport) ──
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags dockFlags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##TumblerDockSpace", nullptr, dockFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("TumblerDockSpace");
    ImGui::DockSpace(dockspaceId);

    // ── Set up initial dock layout (first frame only) ──
    if (!m_DockLayoutSet) {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId,
            ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_NoWindowMenuButton);
        ImGui::DockBuilderSetNodeSize(dockspaceId, vp->Size);

        // Split: left 15% + right 85%
        ImGuiID dockRight;
        ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.15f, nullptr, &dockRight);

        // Split right: bottom 30% (console/inspector) + top 70% (scene/game)
        ImGuiID dockRightBottom;
        ImGuiID dockRightTop = ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.30f, nullptr, &dockRightBottom);

        // Split right-bottom: inspector right 35% + console left 65%
        ImGuiID dockInspector;
        ImGuiID dockConsole = ImGui::DockBuilderSplitNode(dockRightBottom, ImGuiDir_Right, 0.35f, nullptr, &dockInspector);

        // Split right-top: scene main 70% + game view right 30%
        ImGuiID dockGame;
        ImGuiID dockScene = ImGui::DockBuilderSplitNode(dockRightTop, ImGuiDir_Right, 0.30f, nullptr, &dockGame);

        // Dock windows
        ImGui::DockBuilderDockWindow("Scene View",    dockScene);
        ImGui::DockBuilderDockWindow("Game View",     dockGame);
        ImGui::DockBuilderDockWindow("Hierarchy",     dockLeft);
        ImGui::DockBuilderDockWindow("Inspector",     dockInspector);
        ImGui::DockBuilderDockWindow("Console",       dockConsole);
        ImGui::DockBuilderDockWindow("Stats",         dockLeft);   // tab next to Hierarchy

        ImGui::DockBuilderFinish(dockspaceId);
        m_DockLayoutSet = true;
    }

    ImGui::End();

    // ── Draw panels ──
    DrawMenuBar();
    DrawViewPanel("Scene View", m_SceneViewRT, state.sceneViewHovered, state.sceneViewFocused);
    DrawViewPanel("Game View",  m_GameViewRT,  state.gameViewHovered,  state.gameViewFocused);
    DrawHierarchy();
    DrawInspector();
    DrawConsole();
    DrawStatsPanel(fps, clusterCount, triangleCount);

    return state;
}

// ────────────────────────────────────────────────────────────
// Menu Bar
// ────────────────────────────────────────────────────────────

void EditorUI::DrawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) { /* TODO */ }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) { glfwSetWindowShouldClose(glfwGetCurrentContext(), GLFW_TRUE); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Scene View",  nullptr, nullptr, true);   // always shown
            ImGui::MenuItem("Game View",   nullptr, nullptr, true);
            ImGui::MenuItem("Hierarchy",   nullptr, nullptr, true);
            ImGui::MenuItem("Inspector",   nullptr, nullptr, true);
            ImGui::MenuItem("Console",     nullptr, nullptr, true);
            ImGui::MenuItem("Stats",       nullptr, nullptr, true);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) { /* TODO */ }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

// ────────────────────────────────────────────────────────────
// View Panel (Scene / Game)
// ────────────────────────────────────────────────────────────

void EditorUI::DrawViewPanel(const char* title, RenderTarget& rt,
                              bool& outHovered, bool& outFocused) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool open = true;
    ImGui::Begin(title, &open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    outFocused = ImGui::IsWindowFocused();
    outHovered = ImGui::IsWindowHovered();

    // Display the RT — fill available content area
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x > 0 && avail.y > 0 && rt.imguiDescSet != VK_NULL_HANDLE) {
        ImGui::Image((ImTextureID)(intptr_t)rt.imguiDescSet, avail);
        // If mouse is over the image specifically (not just the window chrome)
        if (!ImGui::IsItemHovered()) outHovered = false;
    } else {
        ImGui::TextDisabled("(no render target)");
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

// ────────────────────────────────────────────────────────────
// Hierarchy
// ────────────────────────────────────────────────────────────

void EditorUI::DrawHierarchy() {
    ImGui::Begin("Hierarchy");
    ImGui::TextDisabled("(no scene objects loaded)");
    ImGui::End();
}

// ────────────────────────────────────────────────────────────
// Inspector
// ────────────────────────────────────────────────────────────

void EditorUI::DrawInspector() {
    ImGui::Begin("Inspector");
    if (m_SelectedCluster < 0) {
        ImGui::TextDisabled("Select a cluster to inspect");
    } else {
        ImGui::Text("Cluster %d", m_SelectedCluster);
    }
    ImGui::End();
}

// ────────────────────────────────────────────────────────────
// Console
// ────────────────────────────────────────────────────────────

void EditorUI::DrawConsole() {
    ImGui::Begin("Console");

    // Clear button
    if (ImGui::Button("Clear")) m_LogLines.clear();
    ImGui::SameLine();
    ImGui::Text("%zu lines", m_LogLines.size());

    ImGui::Separator();

    ImGui::BeginChild("LogScroll", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& line : m_LogLines) {
        ImGui::TextUnformatted(line.c_str());
    }
    // Auto-scroll
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    ImGui::End();
}

void EditorUI::AddLog(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    m_LogLines.push_back(buf);
    // Keep last 500 lines
    if (m_LogLines.size() > 500)
        m_LogLines.erase(m_LogLines.begin());
}

// ────────────────────────────────────────────────────────────
// Stats
// ────────────────────────────────────────────────────────────

void EditorUI::DrawStatsPanel(float fps, int clusterCount, int triangleCount) {
    ImGui::Begin("Stats");
    ImGui::Text("FPS:       %.1f", fps);
    ImGui::Text("Frame:     %.2f ms", fps > 0.0f ? 1000.0f / fps : 0.0f);
    ImGui::Separator();
    ImGui::Text("Clusters:  %d", clusterCount);
    ImGui::Text("Triangles: %d", triangleCount);
    if (clusterCount > 0)
        ImGui::Text("Tris/Clus: %.0f", (float)triangleCount / clusterCount);
    ImGui::End();
}

} // namespace Tumbler
