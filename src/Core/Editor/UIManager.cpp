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
    
    InitUIRenderPass(renderer);
    InitUIFramebuffers(renderer);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.ApiVersion = VK_API_VERSION_1_3;
    init_info.Instance = renderer->GetContext().GetInstance();
    init_info.PhysicalDevice = renderer->GetContext().GetPhysicalDevice();
    init_info.Device = device;
    init_info.QueueFamily = renderer->GetContext().GetGraphicsQueueFamily();
    init_info.Queue = renderer->GetContext().GetGraphicsQueue();
    init_info.DescriptorPool = ImGuiPool;
    init_info.PipelineInfoMain.RenderPass = UIRenderPass; // 使用专属 RenderPass
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.MinImageCount = renderer->GetSwapchainImageCount();
    init_info.ImageCount = renderer->GetSwapchainImageCount();
    ImGui_ImplVulkan_Init(&init_info);
}

void UIManager::Cleanup(VkDevice device) {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (UIRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, UIRenderPass, nullptr);
    }
    for (auto fb : UIFramebuffers) {
        vkDestroyFramebuffer(device, fb, nullptr);
    }
    UIFramebuffers.clear();

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

void UIManager::RecordDrawCommands(VkCommandBuffer cmdBuffer, VulkanRenderer* renderer, uint32_t imageIndex) {
    VkRenderPassBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = UIRenderPass;
    info.framebuffer = UIFramebuffers[imageIndex];
    info.renderArea.extent = renderer->GetSwapchainExtent();
    info.renderArea.offset = {0, 0};
    info.clearValueCount = 0;
    
    vkCmdBeginRenderPass(cmdBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
    vkCmdEndRenderPass(cmdBuffer);
}

void UIManager::InitUIRenderPass(VulkanRenderer* renderer) {
    VkAttachmentDescription attachment{};
    attachment.format = renderer->GetSwapchainImageFormat();
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // 继承前面的画面
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // FForwardPipeline 和 FDeferredPipeline 最终输出都是 PRESENT_SRC_KHR，
    // 所以 UI 需要读这个，写完了还是 PRESENT_SRC_KHR
    attachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment{};
    color_attachment.attachment = 0;
    color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &attachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(renderer->GetDevice(), &info, nullptr, &UIRenderPass));
}

void UIManager::InitUIFramebuffers(VulkanRenderer* renderer) {
    const auto& imageViews = renderer->GetSwapchainImageViews();
    VkExtent2D extent = renderer->GetSwapchainExtent();

    UIFramebuffers.resize(imageViews.size());
    for (size_t i = 0; i < imageViews.size(); i++) {
        VkImageView attachments[] = { imageViews[i] };
        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = UIRenderPass;
        info.attachmentCount = 1;
        info.pAttachments = attachments;
        info.width = extent.width;
        info.height = extent.height;
        info.layers = 1;
        VK_CHECK(vkCreateFramebuffer(renderer->GetDevice(), &info, nullptr, &UIFramebuffers[i]));
    }
}
