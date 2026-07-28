#include "Assets/MeshLoader.h"
#include "Core/Platform/AppWindow.h"
#include "Core/Utils/Log.h"
#include "Gfx/CommandManager.h"
#include "Gfx/DeletionQueue.h"
#include "Gfx/DescriptorHeap.h"
#include "Gfx/Swapchain.h"
#include "Gfx/VulkanDevice.h"
#include "Render/Nanite/NaniteBuilder.h"

int main(int argc, char** argv) {
    using namespace Tumbler;

    Log::Init();

    // ---- Nanite test: load mesh + partition ----
    const char* meshPath = (argc > 1) ? argv[1] : "assets/models/Sting-Sword-lowpoly.obj";
    auto mesh = LoadObj(meshPath);
    if (mesh) {
        LOG_INFO("Mesh loaded: {} vertices, {} triangles",
                 mesh->vertices.size(), mesh->indices.size() / 3);

        auto naniteData = Nanite::NaniteBuilder().Build(*mesh);
        if (naniteData) {
            auto& clusters = naniteData->clusterDag.GetClusters();
            LOG_INFO("=== Nanite Partition Result ===");
            LOG_INFO("Total clusters: {}", clusters.size());
            for (size_t c = 0; c < clusters.size(); ++c) {
                LOG_INFO("  Cluster[{}]: {} vertices, {} triangles",
                         c, clusters[c].NumVertices, clusters[c].NumTriangles);
            }
        } else {
            LOG_ERROR("Nanite build failed");
        }
    } else {
        LOG_WARN("Mesh load failed: {}", meshPath);
    }

    const char* scenePath = "assets/scenes/demo.tscene";
    LOG_INFO("Scene: {}", scenePath);

    // ---- Window ----
    AppWindow window;
    auto winResult = window.Init(1920, 1080, "Tumbler");
    if (!winResult) {
        LOG_ERROR("Window init failed");
        return 1;
    }

    // ---- Vulkan Instance ----
    VulkanDevice device;
    auto instResult = device.CreateInstance();
    if (!instResult) {
        LOG_ERROR("Instance creation failed");
        return 1;
    }

    // Surface needs the instance
    auto surface = window.CreateSurface(device.GetInstance());
    if (!surface) {
        LOG_ERROR("Surface creation failed");
        return 1;
    }

    // Physical + logical device (needs surface for queue family queries)
    auto devResult = device.CompleteInit(*surface);
    if (!devResult) {
        LOG_ERROR("Device init failed");
        return 1;
    }

    // ---- Swapchain ----
    Swapchain swapchain;
    int fbW, fbH;
    window.GetFramebufferSize(&fbW, &fbH);
    auto scResult = swapchain.Init(device, *surface, fbW, fbH);
    if (!scResult) {
        LOG_ERROR("Swapchain init failed");
        return 1;
    }

    // ---- Deletion Queue ----
    DeletionQueue deletionQueue;
    deletionQueue.Init(device.GetDevice());

    // ---- Command Manager ----
    CommandManager cmdManager;
    auto cmdResult = cmdManager.Init(device.GetDevice(), device.GetQueueFamilies().graphics);
    if (!cmdResult) {
        LOG_ERROR("CommandManager init failed");
        return 1;
    }

    // ---- Descriptor Heap ----
    DescriptorHeap descriptorHeap;
    auto descResult = descriptorHeap.Init(device.GetDevice(), 1024);
    if (!descResult) {
        LOG_ERROR("DescriptorHeap init failed");
        return 1;
    }

    // ---- Main Loop ----
    LOG_INFO("Entering main loop");
    while (!window.ShouldClose()) {
        window.PollEvents();

        // Handle resize
        if (swapchain.NeedsRecreate()) {
            window.GetFramebufferSize(&fbW, &fbH);
            if (fbW > 0 && fbH > 0) swapchain.Recreate(fbW, fbH);
            continue;
        }

        // Flush completed GPU resources
        deletionQueue.Flush();

        // Acquire swapchain image
        uint32_t imageIndex = 0;
        VkResult acquireResult = swapchain.AcquireNextImage(&imageIndex, VK_NULL_HANDLE, VK_NULL_HANDLE);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR) {
            continue;
        }
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
            break;
        }

        // Record work
        VkCommandBuffer cmd = cmdManager.Allocate();
        VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(cmd, &beginInfo);

        // Transition swapchain image for rendering
        cmdManager.TransitionLayout(cmd, swapchain.GetImage(imageIndex),
                                     VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        // Dynamic rendering: clear to dark gray
        VkRenderingAttachmentInfo colorAttachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = swapchain.GetImageView(imageIndex),
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {.color = {{0.02f, 0.02f, 0.02f, 1.0f}}},
        };

        VkRenderingAttachmentInfo depthAttachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = swapchain.GetDepthView(),
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {.depthStencil = {0.0f, 0}},
        };

        // Transition depth image for rendering
        cmdManager.TransitionLayout(cmd, swapchain.GetDepthImage(),
                                     VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                     VK_IMAGE_ASPECT_DEPTH_BIT);

        VkRenderingInfo renderInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {{0, 0}, swapchain.GetExtent()},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment,
            .pDepthAttachment = &depthAttachment,
        };
        vkCmdBeginRendering(cmd, &renderInfo);

        // TODO: draw calls, ImGui overlay

        vkCmdEndRendering(cmd);

        // Transition swapchain image for present
        cmdManager.TransitionLayout(cmd, swapchain.GetImage(imageIndex),
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        vkEndCommandBuffer(cmd);

        // Submit
        uint64_t signalValue = deletionQueue.AdvanceSubmitCounter();

        VkTimelineSemaphoreSubmitInfo timelineInfo{
            .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
            .signalSemaphoreValueCount = 1,
            .pSignalSemaphoreValues = &signalValue,
        };

        VkSemaphore timelineSem = deletionQueue.GetTimelineSemaphore();
        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = &timelineInfo,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &timelineSem,
        };

        vkQueueSubmit(device.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(device.GetGraphicsQueue());  // simple sync for now

        vkFreeCommandBuffers(device.GetDevice(), cmdManager.GetPool(), 1, &cmd);

        // Present
        VkResult presentResult = swapchain.Present(device.GetPresentQueue(), imageIndex, VK_NULL_HANDLE);
    }

    // ---- Shutdown ----
    vkDeviceWaitIdle(device.GetDevice());

    deletionQueue.Shutdown();
    descriptorHeap.Shutdown();
    cmdManager.Shutdown();
    swapchain.Shutdown();
    device.Shutdown();
    window.Shutdown();

    Log::Shutdown();
    return 0;
}
