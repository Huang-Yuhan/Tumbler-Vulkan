#pragma once

#include <expected>
#include <span>
#include <vector>
#include <vulkan/vulkan.h>

namespace Tumbler {

enum class PipelineError {
    ShaderLoadFailed,
    PipelineCreationFailed,
};

// Load SPIR-V binary from compiled .spv file
std::expected<std::vector<uint32_t>, PipelineError> LoadSpv(const char* path);

// ===================================================================
// ComputePipelineBuilder
// ===================================================================
struct ComputePipelineBuilder {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    const char*      shaderPath = nullptr;  // .spv file

    // Build a single compute pipeline
    std::expected<VkPipeline, PipelineError> Build(VkDevice device);
};

// ===================================================================
// GraphicsPipelineBuilder — for dynamic rendering
// ===================================================================
struct GraphicsPipelineBuilder {
    VkPipelineLayout   layout      = VK_NULL_HANDLE;
    const char*        vertPath    = nullptr;
    const char*        fragPath    = nullptr;
    std::span<VkFormat> colorFormats;  // empty = no color attachments
    VkFormat           depthFormat = VK_FORMAT_UNDEFINED;
    VkCullModeFlags    cullMode    = VK_CULL_MODE_BACK_BIT;
    VkPolygonMode      polygonMode = VK_POLYGON_MODE_FILL;
    VkCompareOp        depthOp     = VK_COMPARE_OP_GREATER;  // reversed-Z
    VkBool32           depthBiasEnable    = VK_FALSE;
    float              depthBiasConstant  = 0.0f;
    float              depthBiasSlope     = 0.0f;

    // Vertex input (empty = no vertex attributes, for GPU-driven rendering)
    std::span<VkVertexInputBindingDescription>   vertexBindings;
    std::span<VkVertexInputAttributeDescription> vertexAttribs;

    std::expected<VkPipeline, PipelineError> Build(VkDevice device);
};

} // namespace Tumbler
