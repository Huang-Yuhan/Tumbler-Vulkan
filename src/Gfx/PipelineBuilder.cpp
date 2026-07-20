#include "PipelineBuilder.h"
#include "Core/Utils/Log.h"

#include <cstdio>
#include <vector>

namespace Tumbler {

std::expected<std::vector<uint32_t>, PipelineError> LoadSpv(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        LOG_ERROR("Failed to open shader: {}", path);
        return std::unexpected(PipelineError::ShaderLoadFailed);
    }
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<uint32_t> code((size + 3) / 4);
    fread(code.data(), 1, size, f);
    fclose(f);
    return code;
}

static VkShaderModule CreateShaderModule(VkDevice device, std::span<const uint32_t> code) {
    VkShaderModuleCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size_bytes(),
        .pCode = code.data(),
    };
    VkShaderModule mod;
    vkCreateShaderModule(device, &info, nullptr, &mod);
    return mod;
}

// ===================================================================
// Compute
// ===================================================================

std::expected<VkPipeline, PipelineError> ComputePipelineBuilder::Build(VkDevice device) {
    auto spv = LoadSpv(shaderPath);
    if (!spv) return std::unexpected(spv.error());

    VkShaderModule mod = CreateShaderModule(device, *spv);

    VkComputePipelineCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = mod,
            .pName = "main",
        },
        .layout = layout,
    };

    VkPipeline pipeline;
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS) {
        LOG_ERROR("Compute pipeline creation failed: {}", shaderPath);
        vkDestroyShaderModule(device, mod, nullptr);
        return std::unexpected(PipelineError::PipelineCreationFailed);
    }

    vkDestroyShaderModule(device, mod, nullptr);
    return pipeline;
}

// ===================================================================
// Graphics
// ===================================================================

std::expected<VkPipeline, PipelineError> GraphicsPipelineBuilder::Build(VkDevice device) {
    auto vertSpv = LoadSpv(vertPath);
    if (!vertSpv) return std::unexpected(vertSpv.error());

    auto fragSpv = LoadSpv(fragPath);
    if (!fragSpv) return std::unexpected(fragSpv.error());

    VkShaderModule vertMod = CreateShaderModule(device, *vertSpv);
    VkShaderModule fragMod = CreateShaderModule(device, *fragSpv);

    VkPipelineShaderStageCreateInfo stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertMod,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragMod,
            .pName = "main",
        },
    };

    // No vertex input — GPU-driven reads vertices from storage buffers
    VkPipelineVertexInputStateCreateInfo vertexInput{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo raster{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = cullMode,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisample{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineDepthStencilStateCreateInfo depthStencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = depthFormat != VK_FORMAT_UNDEFINED ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = depthFormat != VK_FORMAT_UNDEFINED ? VK_TRUE : VK_FALSE,
        .depthCompareOp = depthOp,
    };

    VkPipelineColorBlendAttachmentState blendAttachment{
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(
        colorFormats.size(), blendAttachment);

    VkPipelineColorBlendStateCreateInfo colorBlend{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = static_cast<uint32_t>(blendAttachments.size()),
        .pAttachments = blendAttachments.data(),
    };

    // Dynamic state: viewport + scissor
    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dyn,
    };

    // Dynamic rendering attachment formats
    VkPipelineRenderingCreateInfo rendering{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = static_cast<uint32_t>(colorFormats.size()),
        .pColorAttachmentFormats = colorFormats.data(),
        .depthAttachmentFormat = depthFormat,
    };

    VkGraphicsPipelineCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &raster,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlend,
        .pDynamicState = &dynamicState,
        .layout = layout,
    };

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS) {
        LOG_ERROR("Graphics pipeline creation failed: {} / {}", vertPath, fragPath);
        vkDestroyShaderModule(device, vertMod, nullptr);
        vkDestroyShaderModule(device, fragMod, nullptr);
        return std::unexpected(PipelineError::PipelineCreationFailed);
    }

    vkDestroyShaderModule(device, vertMod, nullptr);
    vkDestroyShaderModule(device, fragMod, nullptr);
    return pipeline;
}

} // namespace Tumbler
