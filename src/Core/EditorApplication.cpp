#include "EditorApplication.h"

#include "Assets/MeshLoader.h"
#include "Core/Platform/AppWindow.h"
#include "Core/Utils/Log.h"
#include "Gfx/CommandManager.h"
#include "Gfx/DeletionQueue.h"
#include "Gfx/Swapchain.h"
#include "Gfx/VulkanDevice.h"
#include "Render/Camera.h"
#include "Render/ClusterRenderer.h"
#include "Render/Nanite/NaniteBuilder.h"
#include "UI/EditorUI.h"
#include "UI/ImGuiLayer.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>

#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

namespace Tumbler {

EditorApplication::EditorApplication() = default;
EditorApplication::~EditorApplication() = default;

// ────────────────────────────────────────────────────────────
// Init
// ────────────────────────────────────────────────────────────

bool EditorApplication::Init(int argc, char** argv) {
    // ── Mesh path ──
    m_MeshPath = (argc > 1) ? argv[1] : ASSET_DIR "/models/bunny.obj";

    // ── Load mesh + build Nanite ──
    auto mesh = LoadObj(m_MeshPath);
    if (!mesh) { LOG_ERROR("Failed to load mesh: {}", m_MeshPath); return false; }
    for (auto& v : mesh->vertices) v.pos *= 10.0f;
    LOG_INFO("Mesh: {} verts, {} tris", mesh->vertices.size(), mesh->indices.size() / 3);

    auto naniteData = Nanite::NaniteBuilder().Build(*mesh);
    if (!naniteData) { LOG_ERROR("Nanite build failed"); return false; }
    auto& clusters = naniteData->clusterDag.GetClusters();
    LOG_INFO("Nanite: {} clusters", clusters.size());

    // ── Window ──
    m_Window = std::make_unique<AppWindow>();
    if (!m_Window->Init(1920, 1080, "Tumbler Editor")) return false;

    // ── Vulkan ──
    m_Device = std::make_unique<VulkanDevice>();
    if (!m_Device->CreateInstance()) return false;
    auto surface = m_Window->CreateSurface(m_Device->GetInstance());
    if (!surface) return false;
    if (!m_Device->CompleteInit(*surface)) return false;
    m_Surface = *surface;

    // ── VMA ──
    VmaAllocatorCreateInfo vmaInfo{
        .physicalDevice   = m_Device->GetPhysicalDevice(),
        .device           = m_Device->GetDevice(),
        .instance         = m_Device->GetInstance(),
        .vulkanApiVersion = VK_API_VERSION_1_4,
    };
    vmaCreateAllocator(&vmaInfo, &m_Allocator);

    // ── Swapchain ──
    m_Swapchain = std::make_unique<Swapchain>();
    int fbW, fbH;
    m_Window->GetFramebufferSize(&fbW, &fbH);
    if (!m_Swapchain->Init(*m_Device, m_Surface, fbW, fbH)) return false;

    // ── Deletion queue ──
    m_DeletionQueue = std::make_unique<DeletionQueue>();
    m_DeletionQueue->Init(m_Device->GetDevice());

    // ── Command manager ──
    m_CmdManager = std::make_unique<CommandManager>();
    if (!m_CmdManager->Init(m_Device->GetDevice(),
                            m_Device->GetQueueFamilies().graphics)) return false;

    // ── Sampler for RT display ──
    VkSamplerCreateInfo samplerInfo{
        .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter    = VK_FILTER_LINEAR,
        .minFilter    = VK_FILTER_LINEAR,
        .mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .maxLod       = 1.0f,
    };
    vkCreateSampler(m_Device->GetDevice(), &samplerInfo, nullptr, &m_RTSampler);

    // ── ImGui (must init before EditorUI — EditorUI calls ImGui_ImplVulkan_AddTexture) ──
    m_ImGui = std::make_unique<ImGuiLayer>();
    ImGuiLayer::Config imguiCfg{
        .window         = m_Window->GetHandle(),
        .instance       = m_Device->GetInstance(),
        .physicalDevice = m_Device->GetPhysicalDevice(),
        .device         = m_Device->GetDevice(),
        .queueFamily    = m_Device->GetQueueFamilies().graphics,
        .queue          = m_Device->GetGraphicsQueue(),
        .minImageCount  = m_Swapchain->GetImageCount(),
        .colorFormat    = m_Swapchain->GetFormat(),
        .depthFormat    = VK_FORMAT_D32_SFLOAT,
    };
    if (!m_ImGui->Init(imguiCfg)) return false;

    // ── Editor UI ──
    m_EditorUI = std::make_unique<EditorUI>();
    if (!m_EditorUI->Init(m_Device->GetDevice(), m_Allocator,
                          m_Swapchain->GetExtent(), m_Swapchain->GetFormat(),
                          m_RTSampler)) return false;

    // ── Cluster renderer ──
    m_ClusterRenderer = std::make_unique<ClusterRenderer>();
    if (!m_ClusterRenderer->Init(m_Device->GetDevice(), m_Allocator,
                                 *m_CmdManager, clusters,
                                 m_Swapchain->GetFormat())) return false;

    // ── Cameras ──
    m_EditorCam = std::make_unique<Camera>();
    m_GameCam   = std::make_unique<Camera>();

    m_EditorCam->SetPerspective(glm::radians(60.0f), 0.1f, 1000.0f);
    m_EditorCam->SetTarget(glm::vec3(0.0f));
    m_EditorCam->SetDistance(5.0f);
    m_EditorCam->Orbit(0.0f, glm::radians(20.0f));

    m_GameCam->SetPerspective(glm::radians(60.0f), 0.1f, 1000.0f);
    m_GameCam->SetTarget(glm::vec3(0.0f));
    m_GameCam->SetDistance(5.0f);
    m_GameCam->Orbit(0.0f, glm::radians(20.0f));

    // ── Fence ──
    VkFenceCreateInfo fenceInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    vkCreateFence(m_Device->GetDevice(), &fenceInfo, nullptr, &m_AcquireFence);

    LOG_INFO("EditorApplication initialized");
    return true;
}

// ────────────────────────────────────────────────────────────
// Run
// ────────────────────────────────────────────────────────────

void EditorApplication::Run() {
    auto TransitionRT = [this](VkCommandBuffer cmd,
                               EditorUI::RenderTarget& rt,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout) {
        VkAccessFlags srcAccess = 0;
        if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkAccessFlags dstAccess = 0;
        if (newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        else if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            dstAccess = VK_ACCESS_SHADER_READ_BIT;

        VkPipelineStageFlags srcStage =
            (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
                ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkPipelineStageFlags dstStage =
            (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkImageMemoryBarrier barrier{
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = srcAccess,
            .dstAccessMask       = dstAccess,
            .oldLayout           = oldLayout,
            .newLayout           = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = rt.colorImage,
            .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        };
        vkCmdPipelineBarrier(cmd, srcStage, dstStage,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
    };

    LOG_INFO("Entering main loop");
    while (!m_Window->ShouldClose()) {
        m_Window->PollEvents();

        if (m_Swapchain->NeedsRecreate()) {
            RecreateSwapchain();
            continue;
        }

        m_DeletionQueue->Flush();

        uint32_t imageIndex = 0;
        VkResult acquireResult = m_Swapchain->AcquireNextImage(
            &imageIndex, VK_NULL_HANDLE, m_AcquireFence);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR ||
            acquireResult == VK_SUBOPTIMAL_KHR) {
            RecreateSwapchain();
            continue;
        }
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
            break;

        vkWaitForFences(m_Device->GetDevice(), 1, &m_AcquireFence, VK_TRUE, UINT64_MAX);
        vkResetFences(m_Device->GetDevice(), 1, &m_AcquireFence);

        // ── ImGui begin ──
        m_ImGui->BeginFrame();

        // ── Editor UI ──
        float fps = ImGui::GetIO().Framerate;
        int numClusters = static_cast<int>(m_ClusterRenderer->GetNumClusters());
        int numTriangles = numClusters * 128; // kClusterTriangleCount
        auto frameState = m_EditorUI->Draw(fps, numClusters, numTriangles);

        // ── Input ──
        float wheel = ImGui::GetIO().MouseWheel;
        float dt    = ImGui::GetIO().DeltaTime;
        HandleInput(frameState, dt, wheel);

        // ── Record ──
        VkCommandBuffer cmd = m_CmdManager->Allocate();
        VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(cmd, &beginInfo);

        // Render Scene View
        RenderView(cmd, m_EditorUI->GetSceneViewRT(), *m_EditorCam);

        // Render Game View
        RenderView(cmd, m_EditorUI->GetGameViewRT(), *m_GameCam);

        // Swapchain → ImGui
        m_CmdManager->TransitionLayout(cmd, m_Swapchain->GetImage(imageIndex),
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        m_CmdManager->TransitionLayout(cmd, m_Swapchain->GetDepthImage(),
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                        VK_IMAGE_ASPECT_DEPTH_BIT);

        VkRenderingAttachmentInfo colorAtt{
            .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView   = m_Swapchain->GetImageView(imageIndex),
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue  = { .color = {{ 0.1f, 0.1f, 0.15f, 1.0f }} },
        };
        VkRenderingAttachmentInfo depthAtt{
            .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView   = m_Swapchain->GetDepthView(),
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue  = { .depthStencil = { 0.0f, 0 } },
        };
        VkRenderingInfo renderInfo{
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea           = {{ 0, 0 }, m_Swapchain->GetExtent()},
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colorAtt,
            .pDepthAttachment     = &depthAtt,
        };
        vkCmdBeginRendering(cmd, &renderInfo);
        m_ImGui->EndFrame(cmd);
        vkCmdEndRendering(cmd);

        m_CmdManager->TransitionLayout(cmd, m_Swapchain->GetImage(imageIndex),
                                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        vkEndCommandBuffer(cmd);

        // ── Submit ──
        uint64_t signalValue = m_DeletionQueue->AdvanceSubmitCounter();
        VkTimelineSemaphoreSubmitInfo timelineInfo{
            .sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
            .signalSemaphoreValueCount = 1,
            .pSignalSemaphoreValues    = &signalValue,
        };
        VkSemaphore timelineSem = m_DeletionQueue->GetTimelineSemaphore();
        VkSubmitInfo submitInfo{
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                = &timelineInfo,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &cmd,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores    = &timelineSem,
        };
        vkQueueSubmit(m_Device->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_Device->GetGraphicsQueue());
        vkFreeCommandBuffers(m_Device->GetDevice(),
                             m_CmdManager->GetPool(), 1, &cmd);

        m_Swapchain->Present(m_Device->GetPresentQueue(), imageIndex,
                             VK_NULL_HANDLE);
    }
}

// ────────────────────────────────────────────────────────────
// Shutdown
// ────────────────────────────────────────────────────────────

void EditorApplication::Shutdown() {
    vkDeviceWaitIdle(m_Device->GetDevice());

    if (m_AcquireFence) {
        vkDestroyFence(m_Device->GetDevice(), m_AcquireFence, nullptr);
        m_AcquireFence = VK_NULL_HANDLE;
    }

    m_ClusterRenderer->Shutdown(*m_DeletionQueue);
    m_ClusterRenderer.reset();
    m_EditorCam.reset();
    m_GameCam.reset();

    m_EditorUI->Shutdown();
    m_EditorUI.reset();

    m_ImGui->Shutdown();
    m_ImGui.reset();

    if (m_RTSampler) {
        vkDestroySampler(m_Device->GetDevice(), m_RTSampler, nullptr);
        m_RTSampler = VK_NULL_HANDLE;
    }

    m_DeletionQueue->Shutdown();
    m_CmdManager->Shutdown();
    m_Swapchain->Shutdown();
    vmaDestroyAllocator(m_Allocator);
    m_Allocator = VK_NULL_HANDLE;

    vkDestroySurfaceKHR(m_Device->GetInstance(), m_Surface, nullptr);
    m_Device->Shutdown();
    m_Window->Shutdown();

    LOG_INFO("EditorApplication shutdown");
}

// ────────────────────────────────────────────────────────────
// Private helpers
// ────────────────────────────────────────────────────────────

void EditorApplication::RenderView(VkCommandBuffer cmd,
                                    EditorUI::RenderTarget& rt,
                                    const Camera& camera) {
    VkDevice dev = m_Device->GetDevice();

    // Color: UNDEFINED → COLOR_ATTACHMENT
    VkImageMemoryBarrier colorBarrier{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = rt.colorImage,
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &colorBarrier);

    // Depth: UNDEFINED → DEPTH_ATTACHMENT
    VkImageMemoryBarrier depthBarrier{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask       = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = rt.depthImage,
        .subresourceRange    = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 },
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &depthBarrier);

    // Dynamic rendering
    VkRenderingAttachmentInfo colorAtt{
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = rt.colorView,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue  = { .color = {{ 0.1f, 0.1f, 0.15f, 1.0f }} },
    };
    VkRenderingAttachmentInfo depthAtt{
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = rt.depthView,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp     = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue  = { .depthStencil = { 0.0f, 0 } },
    };
    VkRenderingInfo renderInfo{
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = {{ 0, 0 }, rt.extent},
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colorAtt,
        .pDepthAttachment     = &depthAtt,
    };
    vkCmdBeginRendering(cmd, &renderInfo);

    float aspect = static_cast<float>(rt.extent.width) /
                   static_cast<float>(rt.extent.height);
    Camera viewCam = camera;
    viewCam.SetAspectRatio(aspect);
    m_ClusterRenderer->Render(cmd, viewCam.GetViewProjection(),
                              ClusterRenderer::Mode::ShadedWireframe,
                              rt.extent);

    vkCmdEndRendering(cmd);

    // Color: COLOR_ATTACHMENT → SHADER_READ_ONLY (for ImGui)
    VkImageMemoryBarrier toReadBarrier{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = rt.colorImage,
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toReadBarrier);
}

void EditorApplication::HandleInput(const EditorUI::FrameState& frameState,
                                     float dt, float wheel) {
    // Wheel → zoom focused view
    if (wheel != 0.0f && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
        if (frameState.sceneViewHovered)
            m_EditorCam->Zoom(wheel * 2.0f);
        else if (frameState.gameViewHovered)
            m_GameCam->Zoom(wheel * 2.0f);
    }

    // Right-drag → orbit focused view
    if (frameState.sceneViewHovered && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
        m_EditorCam->Orbit(delta.x * 0.005f, delta.y * 0.005f);
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
    }
    if (frameState.gameViewHovered && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
        m_GameCam->Orbit(delta.x * 0.005f, delta.y * 0.005f);
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
    }
}

void EditorApplication::RecreateSwapchain() {
    int fbW, fbH;
    m_Window->GetFramebufferSize(&fbW, &fbH);
    if (fbW <= 0 || fbH <= 0) return;

    m_Swapchain->Recreate(fbW, fbH);
    m_ImGui->OnSwapchainRecreate(m_Swapchain->GetImageCount());
    m_EditorUI->OnResize(m_Swapchain->GetExtent());
}

} // namespace Tumbler
