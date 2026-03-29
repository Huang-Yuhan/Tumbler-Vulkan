#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <functional>
#include "SceneViewData.h"
#include "RenderPacket.h"

// Forward Declaration
class VulkanRenderer;

/**
 * @brief 渲染管线基类 (Render Pipeline Strategy Interface)
 * 封装并管理独立的 RenderPass 与管线资源（例如多屏或特定于渲染路径的数据）。
 */
class IRenderPipeline {
public:
    virtual ~IRenderPipeline() = default;

    /**
     * @brief 初始化管线资源（RenderPass 等）
     */
    virtual void Init(VulkanRenderer* renderer) = 0;

    /**
     * @brief 清理管线自身分配的内部结构
     */
    virtual void Cleanup(VulkanRenderer* renderer) = 0;

    /**
     * @brief 当 Swapchain 重建时触发 (重建 Framebuffers 及相关的 G-Buffer 等)
     */
    virtual void RecreateResources(VulkanRenderer* renderer) = 0;

    /**
     * @brief 录制该管线下的核心 Draw Cmds
     */
    virtual void RecordCommands(
        VkCommandBuffer cmd,
        uint32_t imageIndex,
        VulkanRenderer* renderer,
        const SceneViewData& viewData,
        const std::vector<RenderPacket>& renderPackets,
        std::function<void(VkCommandBuffer)> onUIRender) = 0;

    /**
     * @brief 获取管线对应的 RenderPass
     */
    virtual VkRenderPass GetRenderPass() const = 0;
};
