#include "VulkanPipelineBuilder.h"
#include <iostream>
#include <stdexcept>

VulkanPipelineBuilder VulkanPipelineBuilder::Begin(VkPipelineLayout layout) {
    VulkanPipelineBuilder builder;
    builder.PipelineLayout = layout;
    // 初始化 VertexInputInfo，防止 Build 时为空指针 (虽然目前我们还没设置具体 Attribute)
    builder.VertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    return builder;
}

VkPipeline VulkanPipelineBuilder::Build(VkDevice device, VkRenderPass renderPass) {
    // 1. 组装视口状态 (Viewport State)
    // 虽然 Viewport 和 Scissor 已经在成员变量里了，但需要用这个结构体把它们包起来
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &Viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &Scissor;

    // 2. 组装颜色混合状态 (Global Color Blend State)
    // 这是一个全局设置，可以控制逻辑操作 (LogicOp)，并引用具体的 Attachment
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE; // 不使用位运算混合
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &ColorBlendAttachment;

    VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
    dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(DynamicStates.size());
    dynamicStateInfo.pDynamicStates = DynamicStates.empty() ? nullptr : DynamicStates.data();

    // 3. 最终的大结构体：Graphics Pipeline Create Info
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    
    // 链接所有 Shader 阶段
    pipelineInfo.stageCount = static_cast<uint32_t>(ShaderStages.size());
    pipelineInfo.pStages = ShaderStages.data();
    
    // 链接所有固定功能状态 (Fixed-Function State)
    pipelineInfo.pVertexInputState = &VertexInputInfo;
    pipelineInfo.pInputAssemblyState = &InputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &Rasterizer;
    pipelineInfo.pMultisampleState = &Multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &DepthStencil;
    pipelineInfo.pDynamicState = DynamicStates.empty() ? nullptr : &dynamicStateInfo;
    
    // 链接布局和 RenderPass
    pipelineInfo.layout = PipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0; // 这个管线用于 RenderPass 的第 0 个子流程
    
    // 允许管线派生 (优化创建速度)，这里暂时不用
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    // 4. 调用 Vulkan API 创建管线
    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }
    
    return newPipeline;
}

// =================================================================
// 链式配置方法的实现 (Setter 返回 *this)
// =================================================================

VulkanPipelineBuilder& VulkanPipelineBuilder::SetShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader) {
    ShaderStages.clear();

    // 顶点着色器阶段
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertexShader;
    vertShaderStageInfo.pName = "main"; // 入口函数名
    ShaderStages.push_back(vertShaderStageInfo);

    // 片元着色器阶段
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragmentShader;
    fragShaderStageInfo.pName = "main";
    ShaderStages.push_back(fragShaderStageInfo);

    return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetViewport(uint32_t width, uint32_t height) {
    // 视口：将 (-1, 1) 的标准化坐标映射到屏幕像素坐标 (0, width)
    Viewport.x = 0.0f;
    Viewport.y = 0.0f;
    Viewport.width = (float)width;
    Viewport.height = (float)height;
    Viewport.minDepth = 0.0f; // 最小深度值 (0.0)
    Viewport.maxDepth = 1.0f; // 最大深度值 (1.0)

    // 裁剪：只保留这个矩形内的像素
    Scissor.offset = {0, 0};
    Scissor.extent = {width, height};

    return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetDynamicViewportScissor() {
    DynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetInputTopology(VkPrimitiveTopology topology) {
    InputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    InputAssembly.topology = topology; // 例如：三角形列表、线条列表
    InputAssembly.primitiveRestartEnable = VK_FALSE; // 如果为 True，可以用特殊索引断开图元
    return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetPolygonMode(VkPolygonMode mode) {
    Rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    Rasterizer.depthClampEnable = VK_FALSE; // 超出深度的片段是否被截断而不是丢弃
    Rasterizer.rasterizerDiscardEnable = VK_FALSE; // 如果为 True，几何体永远不会光栅化到帧缓冲
    
    Rasterizer.polygonMode = mode; // FILL (填充), LINE (线框), POINT (点)
    Rasterizer.lineWidth = 1.0f;   // 线宽 (大于1需要开启GPU特性)
    
    return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace) {
    Rasterizer.cullMode = cullMode;   // 剔除哪一面？(BACK, FRONT, NONE)
    Rasterizer.frontFace = frontFace; // 顶点的顺时针(CW)还是逆时针(CCW)算正面？
    return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetMultisamplingNone() {
    Multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    Multisampling.sampleShadingEnable = VK_FALSE;
    Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // 1个样本 = 无抗锯齿
    Multisampling.minSampleShading = 1.0f;
    Multisampling.pSampleMask = nullptr;
    Multisampling.alphaToCoverageEnable = VK_FALSE;
    Multisampling.alphaToOneEnable = VK_FALSE;
    return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetColorBlending(bool enableBlend) {
    // 这里的 mask 决定了输出颜色的哪些通道会被写入 (RGBA 全写)
    ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    
    if (enableBlend) {
        // 标准的 Alpha 混合公式：
        // FinalColor = SrcColor * SrcAlpha + DstColor * (1 - SrcAlpha)
        ColorBlendAttachment.blendEnable = VK_TRUE;
        
        ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        
        ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    } else {
        ColorBlendAttachment.blendEnable = VK_FALSE;
    }
    return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::DisableDepthTest() {
    DepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    DepthStencil.depthTestEnable = VK_FALSE;   // 不比较深度
    DepthStencil.depthWriteEnable = VK_FALSE;  // 不写入深度
    DepthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    DepthStencil.depthBoundsTestEnable = VK_FALSE;
    DepthStencil.stencilTestEnable = VK_FALSE; // 不使用模板测试
    return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::EnableDepthTest(bool depthWriteEnable, VkCompareOp op) {
    DepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    DepthStencil.depthTestEnable = VK_TRUE;
    DepthStencil.depthWriteEnable = depthWriteEnable ? VK_TRUE : VK_FALSE;
    DepthStencil.depthCompareOp = op; // 通常是 VK_COMPARE_OP_LESS (近的盖住远的)
    DepthStencil.depthBoundsTestEnable = VK_FALSE;
    DepthStencil.stencilTestEnable = VK_FALSE;
    return *this;
}
