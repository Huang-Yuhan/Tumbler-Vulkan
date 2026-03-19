#pragma once

#include <vulkan/vulkan.h>
#include <vector>

/**
 * @brief Vulkan 图形管线构建器
 * * Vulkan 的管线创建非常繁琐，需要填充大量的结构体。
 * 这个类使用 Builder 模式封装了这些配置过程，支持链式调用。
 */
class VulkanPipelineBuilder {
public:
    // ==========================================
    // 管线状态积木 (State Components)
    // 这些成员变量暂存了管线的各项配置，直到 Build() 被调用
    // ==========================================

    // 着色器阶段 (Vertex, Fragment 等)
    std::vector<VkPipelineShaderStageCreateInfo> ShaderStages;

    // 顶点输入 (Layout, Attribute)
    VkPipelineVertexInputStateCreateInfo VertexInputInfo{};

    // 输入装配 (点、线、三角形列表)
    VkPipelineInputAssemblyStateCreateInfo InputAssembly{};

    // 视口 (渲染区域)
    VkViewport Viewport{};

    // 裁剪矩形 (只保留这个矩形内的像素)
    VkRect2D Scissor{};

    // 光栅化 (填充模式、面剔除、线宽)
    VkPipelineRasterizationStateCreateInfo Rasterizer{};

    // 多重采样 (抗锯齿)
    VkPipelineMultisampleStateCreateInfo Multisampling{};

    // 颜色混合 (透明度混合设置)
    VkPipelineColorBlendAttachmentState ColorBlendAttachment{};

    // 深度/模板测试
    VkPipelineDepthStencilStateCreateInfo DepthStencil{};
    std::vector<VkDynamicState> DynamicStates;

    // 管线布局 (Push Constants, Descriptor Sets)
    VkPipelineLayout PipelineLayout{};

    // ==========================================
    // 核心接口
    // ==========================================

    /**
     * @brief 开始构建一个新的管线配置
     * @param layout 管线布局 (Pipeline Layout)
     * @return 返回一个新的 Builder 实例
     */
    static VulkanPipelineBuilder Begin(VkPipelineLayout layout);

    /**
     * @brief 组装所有积木，通知显卡创建管线对象
     * @param device 逻辑设备
     * @param renderPass 关联的渲染通道
     * @return 创建成功的 VkPipeline 句柄
     */
    VkPipeline Build(VkDevice device, VkRenderPass renderPass);

    // ==========================================
    // 配置方法 (支持链式调用)
    // ==========================================

    // 设置顶点和片元着色器
    VulkanPipelineBuilder& SetShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);

    // 设置视口大小 (通常和 Swapchain 图像大小一致)
    VulkanPipelineBuilder& SetViewport(uint32_t width, uint32_t height);
    VulkanPipelineBuilder& SetDynamicViewportScissor();

    // 设置拓扑结构 (例如 VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
    VulkanPipelineBuilder& SetInputTopology(VkPrimitiveTopology topology);

    // 设置多边形模式 (VK_POLYGON_MODE_FILL / LINE / POINT)
    VulkanPipelineBuilder& SetPolygonMode(VkPolygonMode mode);

    // 设置剔除模式 (例如剔除背面 VK_CULL_MODE_BACK_BIT)
    VulkanPipelineBuilder& SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);

    // 关闭多重采样 (1 sample)
    VulkanPipelineBuilder& SetMultisamplingNone();

    // 设置颜色混合 (是否开启 Alpha Blending)
    VulkanPipelineBuilder& SetColorBlending(bool enableBlend);

    // 关闭深度测试 (用于 2D UI 或背景)
    VulkanPipelineBuilder& DisableDepthTest();

    VulkanPipelineBuilder& EnableDepthTest(bool depthWriteEnable, VkCompareOp op);
};
