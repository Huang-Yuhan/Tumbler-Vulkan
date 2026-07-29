#include "ImGuiLayer.h"
#include "Core/Utils/Log.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>

namespace Tumbler {

bool ImGuiLayer::Init(const Config& config) {
    // ---- Context ----
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;  // don't save/load window positions

    // Dark style
    ImGui::StyleColorsDark();

    // ---- GLFW backend ----
    if (!ImGui_ImplGlfw_InitForVulkan(config.window, true)) {
        LOG_ERROR("ImGui_ImplGlfw_InitForVulkan failed");
        ImGui::DestroyContext();
        return false;
    }

    // ---- Vulkan backend (Dynamic Rendering) ----
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
    initInfo.DescriptorPoolSize = 100;  // let ImGui create its own pool
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
    LOG_INFO("ImGuiLayer initialized");
    return true;
}

void ImGuiLayer::Shutdown() {
    if (!m_Initialized) return;

    // Flush pending GPU work before destroying ImGui resources
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
}

void ImGuiLayer::OnSwapchainRecreate(uint32_t minImageCount) {
    ImGui_ImplVulkan_SetMinImageCount(minImageCount);
}

} // namespace Tumbler
