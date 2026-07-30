#include "ImGuiLayer.h"
#include "Core/Utils/Log.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>

namespace Tumbler {

void ImGuiLayer::ApplyUnityTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // ── Rounding & spacing ──
    style.WindowRounding    = 4.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 3.0f;
    style.GrabRounding      = 3.0f;
    style.PopupRounding     = 3.0f;
    style.ScrollbarRounding = 6.0f;
    style.TabRounding       = 4.0f;
    style.WindowPadding     = ImVec2(8, 8);
    style.FramePadding      = ImVec2(6, 4);
    style.ItemSpacing       = ImVec2(8, 6);
    style.CellPadding       = ImVec2(6, 4);
    style.WindowBorderSize  = 0.0f;

    // ── Unity-like dark palette ──
    // Base backgrounds
    colors[ImGuiCol_WindowBg]             = ImVec4(0.145f, 0.145f, 0.145f, 1.00f);  // #252525
    colors[ImGuiCol_ChildBg]              = ImVec4(0.145f, 0.145f, 0.145f, 1.00f);
    colors[ImGuiCol_PopupBg]              = ImVec4(0.18f, 0.18f, 0.18f, 0.98f);     // #2E2E2E
    colors[ImGuiCol_Border]               = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);     // #1C1C1C
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Title bar
    colors[ImGuiCol_TitleBg]              = ImVec4(0.176f, 0.176f, 0.176f, 1.00f);  // #2D2D2D
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.220f, 0.220f, 0.220f, 1.00f);  // #383838
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.145f, 0.145f, 0.145f, 1.00f);

    // Menu bar
    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.165f, 0.165f, 0.165f, 1.00f);  // #2A2A2A

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);

    // Slider
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.56f, 0.56f, 0.56f, 1.00f);

    // Button
    colors[ImGuiCol_Button]               = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);     // #383838
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);     // #474747
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.34f, 0.34f, 0.34f, 1.00f);     // #575757

    // Header / collapsing header
    colors[ImGuiCol_Header]               = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.34f, 0.34f, 0.34f, 1.00f);

    // Separator
    colors[ImGuiCol_Separator]            = ImVec4(0.22f, 0.22f, 0.22f, 0.80f);
    colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.34f, 0.34f, 0.34f, 1.00f);
    colors[ImGuiCol_SeparatorActive]      = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);

    // Resize grip
    colors[ImGuiCol_ResizeGrip]           = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.34f, 0.34f, 0.34f, 1.00f);
    colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);

    // Tabs
    colors[ImGuiCol_Tab]                  = ImVec4(0.165f, 0.165f, 0.165f, 1.00f);  // #2A2A2A
    colors[ImGuiCol_TabHovered]           = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_TabActive]            = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_TabUnfocused]         = ImVec4(0.145f, 0.145f, 0.145f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_TabSelectedOverline]  = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);

    // Docking
    colors[ImGuiCol_DockingPreview]       = ImVec4(0.40f, 0.40f, 0.40f, 0.60f);
    colors[ImGuiCol_DockingEmptyBg]       = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);

    // Frame / input field background
    colors[ImGuiCol_FrameBg]              = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);

    // Checkbox / Radio
    colors[ImGuiCol_CheckMark]            = ImVec4(0.64f, 0.64f, 0.64f, 1.00f);

    // Plot
    colors[ImGuiCol_PlotLines]            = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]     = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_PlotHistogram]        = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);

    // Table
    colors[ImGuiCol_TableHeaderBg]        = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]    = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_TableBorderLight]     = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]        = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);

    // Text
    colors[ImGuiCol_Text]                 = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);

    // Nav highlight
    colors[ImGuiCol_NavHighlight]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
}

bool ImGuiLayer::Init(const Config& config) {
    // ── Context ──
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // multi-window support (floating panels)
    io.IniFilename = nullptr;

    // ── Unity theme ──
    ApplyUnityTheme();

    // When viewports are enabled, tweak platform window appearance
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 1.0f;  // opaque for viewport windows
    }

    // ── GLFW backend ──
    if (!ImGui_ImplGlfw_InitForVulkan(config.window, true)) {
        LOG_ERROR("ImGui_ImplGlfw_InitForVulkan failed");
        ImGui::DestroyContext();
        return false;
    }

    // ── Vulkan backend (Dynamic Rendering) ──
    VkPipelineRenderingCreateInfoKHR pipelineRenderingInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &config.colorFormat,
        .depthAttachmentFormat = config.depthFormat,
    };

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion       = VK_API_VERSION_1_4;
    initInfo.Instance         = config.instance;
    initInfo.PhysicalDevice   = config.physicalDevice;
    initInfo.Device           = config.device;
    initInfo.QueueFamily      = config.queueFamily;
    initInfo.Queue            = config.queue;
    initInfo.DescriptorPool   = VK_NULL_HANDLE;
    initInfo.DescriptorPoolSize = 100;
    initInfo.MinImageCount    = config.minImageCount;
    initInfo.ImageCount       = config.minImageCount;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingInfo;
    initInfo.CheckVkResultFn  = [](VkResult err) {
        if (err != VK_SUCCESS) LOG_ERROR("ImGui Vulkan error: {}", static_cast<int>(err));
    };

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        LOG_ERROR("ImGui_ImplVulkan_Init failed");
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    m_Device      = config.device;
    m_Initialized = true;
    LOG_INFO("ImGuiLayer initialized (Docking + Unity theme)");
    return true;
}

void ImGuiLayer::Shutdown() {
    if (!m_Initialized) return;

    vkDeviceWaitIdle(m_Device);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    m_Device      = VK_NULL_HANDLE;
    m_Initialized  = false;
    LOG_INFO("ImGuiLayer shutdown");
}

void ImGuiLayer::BeginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::EndFrame(VkCommandBuffer cmd) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    // Update and render additional platform windows (for viewport feature)
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void ImGuiLayer::OnSwapchainRecreate(uint32_t minImageCount) {
    ImGui_ImplVulkan_SetMinImageCount(minImageCount);
}

} // namespace Tumbler
