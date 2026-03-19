#include "UIManager.h"
#include "Core/Graphics/VulkanRenderer.h"
#include "Core/Platform/AppWindow.h"
#include "Core/Utils/VulkanUtils.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <array>

#include "Core/Utils/Log.h"

void UIManager::Init(AppWindow* window, VulkanRenderer* renderer) {
    VkDevice device = renderer->GetDevice();

    // 1. 创建 ImGui 专属 Descriptor Pool
    std::array<VkDescriptorPoolSize, 11> pool_sizes = {
        {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
         {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
         {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
         {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}}
    };

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();

    VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &ImGuiPool));

    // 2. 初始化 ImGui 上下文
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // 3. 配置 GLFW 和 Vulkan 后端
    ImGui_ImplGlfw_InitForVulkan(window->GetNativeWindow(), true);
    
    // 注意：这里需要 VulkanRenderer 开放 GetContext() 方法，如果没有的话，稍后在 Renderer 里补一个
    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = renderer->GetContext().GetInstance();
    init_info.PhysicalDevice = renderer->GetContext().GetPhysicalDevice();
    init_info.Device = device;
    init_info.QueueFamily = renderer->GetContext().GetGraphicsQueueFamily();
    init_info.Queue = renderer->GetContext().GetGraphicsQueue();
    init_info.DescriptorPool = ImGuiPool;
    init_info.RenderPass = renderer->GetRenderPass();
    init_info.MinImageCount = renderer->GetSwapchainImageCount();
    init_info.ImageCount = renderer->GetSwapchainImageCount();
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&init_info);

    // 4. 上传字体纹理
    ImGui_ImplVulkan_CreateFontsTexture();
}

void UIManager::Cleanup(VkDevice device) {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (ImGuiPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, ImGuiPool, nullptr);
    }
}

void UIManager::BeginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void UIManager::EndFrame() {
    ImGui::Render();
}

void UIManager::RecordDrawCommands(VkCommandBuffer cmdBuffer) {
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
}
