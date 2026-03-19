#include "FMaterial.h"
#include "FMaterialInstance.h"
#include "Core/Graphics/VulkanRenderer.h"
#include "Core/Graphics/VulkanPipelineBuilder.h"
#include "Core/Utils/Log.h"
#include <stdexcept>

FMaterial::FMaterial(VulkanRenderer* renderer, FAssetManager* assetMgr) : Renderer(renderer), AssetManager(assetMgr) {
}

FMaterial::~FMaterial() {
    if (Renderer == VK_NULL_HANDLE) return;
    VkDevice device = Renderer->GetDevice();
    if (Pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, Pipeline, nullptr);
    if (PipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, PipelineLayout, nullptr);
    if (DescriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, DescriptorSetLayout, nullptr);
    
    LOG_INFO("Material Template Destroyed");
}

void FMaterial::BuildPipeline(const std::string& vertPath, const std::string& fragPath) {
    VkDevice device = Renderer->GetDevice();

    // 1. 创建 Descriptor Set Layout (两个 Binding: 贴图 + UBO)
    VkDescriptorSetLayoutBinding textureBinding{};
    textureBinding.binding = 0;
    textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureBinding.descriptorCount = 1;
    textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 1;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // 在片元着色器中使用

    VkDescriptorSetLayoutBinding bindings[] = {textureBinding, uboBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &DescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }

    // 2. 创建 Pipeline Layout (预留 MVP 矩阵的 Push Constant)
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayout layouts[] = { Renderer->GetGlobalSetLayout(), DescriptorSetLayout };


    VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    pipelineLayoutInfo.setLayoutCount = 2;
    pipelineLayoutInfo.pSetLayouts = layouts;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &PipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    // 3. 加载 Shader
    VkShaderModule vertModule, fragModule;
    if (!Renderer->LoadShaderModule(vertPath.c_str(), &vertModule) ||
        !Renderer->LoadShaderModule(fragPath.c_str(), &fragModule)) {
        throw std::runtime_error("Failed to load shaders for material!");
    }

    // 4. 构建管线 (加入对 PBR 法线的支持)
    auto builder = VulkanPipelineBuilder::Begin(PipelineLayout);
    
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = 8 * sizeof(float); // Pos(3) + Norm(3) + UV(2) = 8
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attributeDescriptions = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},                             // Location 0: Position
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof(float)},             // Location 1: Normal
        {2, 0, VK_FORMAT_R32G32_SFLOAT, 6 * sizeof(float)}                 // Location 2: UV
    };

    builder.VertexInputInfo.vertexBindingDescriptionCount = 1;
    builder.VertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    builder.VertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    builder.VertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    Pipeline = builder
        .SetShaders(vertModule, fragModule)
        .SetViewport(Renderer->GetSwapchainExtent().width, Renderer->GetSwapchainExtent().height)
        .SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_FILL)
        .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE) // 测试期先关掉剔除
        .SetMultisamplingNone()
        .EnableDepthTest(VK_TRUE, VK_COMPARE_OP_LESS)
        .SetColorBlending(true)
        .Build(device, Renderer->GetRenderPass());

    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);
    LOG_INFO("PBR Material Pipeline Built Successfully");
}

std::shared_ptr<FMaterialInstance> FMaterial::CreateInstance() {
    VkDescriptorSet newSet = Renderer->AllocateDescriptorSet(DescriptorSetLayout);
    return std::make_shared<FMaterialInstance>(shared_from_this(), Renderer, AssetManager, newSet);
}