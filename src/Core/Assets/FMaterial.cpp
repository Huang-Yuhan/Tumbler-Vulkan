#include "FMaterial.h"
#include "FMaterialInstance.h"
#include "Core/Graphics/RenderDevice.h"
#include "Core/Graphics/VulkanPipelineBuilder.h"
#include "Core/Graphics/ResourceUploadManager.h"
#include "Core/Assets/FAssetManager.h"
#include "Core/Graphics/VulkanRenderer.h"
#include "Core/Utils/Log.h"
#include <stdexcept>
#include <fstream>

FMaterial::FMaterial(
    RenderDevice* renderDevice,
    VkRenderPass renderPass,
    VkDescriptorSetLayout globalSetLayout,
    VkExtent2D swapchainExtent,
    FAssetManager* assetMgr,
    VulkanRenderer* renderer)
    : RenderDeviceRef(renderDevice)
    , RenderPass(renderPass)
    , GlobalSetLayout(globalSetLayout)
    , SwapchainExtent(swapchainExtent)
    , AssetManager(assetMgr)
    , Renderer(renderer) {}

FMaterial::~FMaterial() {
    if (RenderDeviceRef) {
        VkDevice device = RenderDeviceRef->GetDevice();
        if (Pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, Pipeline, nullptr);
            Pipeline = VK_NULL_HANDLE;
        }
        if (PipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, PipelineLayout, nullptr);
            PipelineLayout = VK_NULL_HANDLE;
        }
        if (DescriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, DescriptorSetLayout, nullptr);
            DescriptorSetLayout = VK_NULL_HANDLE;
        }
    }
    LOG_INFO("Material Template Destroyed");
}

FMaterial::FMaterial(FMaterial&& other) noexcept
    : Pipeline(other.Pipeline)
    , PipelineLayout(other.PipelineLayout)
    , DescriptorSetLayout(other.DescriptorSetLayout)
    , RenderDeviceRef(other.RenderDeviceRef)
    , RenderPass(other.RenderPass)
    , GlobalSetLayout(other.GlobalSetLayout)
    , SwapchainExtent(other.SwapchainExtent)
    , AssetManager(other.AssetManager)
    , Renderer(other.Renderer) {
    other.Pipeline = VK_NULL_HANDLE;
    other.PipelineLayout = VK_NULL_HANDLE;
    other.DescriptorSetLayout = VK_NULL_HANDLE;
    other.RenderDeviceRef = nullptr;
    other.RenderPass = VK_NULL_HANDLE;
    other.GlobalSetLayout = VK_NULL_HANDLE;
    other.AssetManager = nullptr;
    other.Renderer = nullptr;
}

FMaterial& FMaterial::operator=(FMaterial&& other) noexcept {
    if (this != &other) {
        if (RenderDeviceRef) {
            VkDevice device = RenderDeviceRef->GetDevice();
            if (Pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, Pipeline, nullptr);
            if (PipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, PipelineLayout, nullptr);
            if (DescriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, DescriptorSetLayout, nullptr);
        }

        Pipeline = other.Pipeline;
        PipelineLayout = other.PipelineLayout;
        DescriptorSetLayout = other.DescriptorSetLayout;
        RenderDeviceRef = other.RenderDeviceRef;
        RenderPass = other.RenderPass;
        GlobalSetLayout = other.GlobalSetLayout;
        SwapchainExtent = other.SwapchainExtent;
        AssetManager = other.AssetManager;
        Renderer = other.Renderer;

        other.Pipeline = VK_NULL_HANDLE;
        other.PipelineLayout = VK_NULL_HANDLE;
        other.DescriptorSetLayout = VK_NULL_HANDLE;
        other.RenderDeviceRef = nullptr;
        other.RenderPass = VK_NULL_HANDLE;
        other.GlobalSetLayout = VK_NULL_HANDLE;
        other.AssetManager = nullptr;
        other.Renderer = nullptr;
    }
    return *this;
}

void FMaterial::BuildPipeline(const std::string& vertPath, const std::string& fragPath) {
    if (!RenderDeviceRef) {
        throw std::runtime_error("RenderDevice is null!");
    }

    VkDevice device = RenderDeviceRef->GetDevice();

    VkDescriptorSetLayoutBinding textureBinding{};
    textureBinding.binding = 0;
    textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureBinding.descriptorCount = 1;
    textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 1;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bindings[] = {textureBinding, uboBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &DescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayout layouts[] = {GlobalSetLayout, DescriptorSetLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    pipelineLayoutInfo.setLayoutCount = 2;
    pipelineLayoutInfo.pSetLayouts = layouts;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &PipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    VkShaderModule vertModule = VK_NULL_HANDLE;
    VkShaderModule fragModule = VK_NULL_HANDLE;

    std::ifstream vertFile(vertPath, std::ios::ate | std::ios::binary);
    if (!vertFile.is_open()) {
        throw std::runtime_error("Failed to open vertex shader: " + vertPath);
    }
    size_t vertSize = static_cast<size_t>(vertFile.tellg());
    std::vector<char> vertBuffer(vertSize);
    vertFile.seekg(0);
    vertFile.read(vertBuffer.data(), vertSize);
    vertFile.close();

    VkShaderModuleCreateInfo vertCreateInfo{};
    vertCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertCreateInfo.codeSize = vertBuffer.size();
    vertCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertBuffer.data());

    if (vkCreateShaderModule(device, &vertCreateInfo, nullptr, &vertModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vertex shader module!");
    }

    std::ifstream fragFile(fragPath, std::ios::ate | std::ios::binary);
    if (!fragFile.is_open()) {
        vkDestroyShaderModule(device, vertModule, nullptr);
        throw std::runtime_error("Failed to open fragment shader: " + fragPath);
    }
    size_t fragSize = static_cast<size_t>(fragFile.tellg());
    std::vector<char> fragBuffer(fragSize);
    fragFile.seekg(0);
    fragFile.read(fragBuffer.data(), fragSize);
    fragFile.close();

    VkShaderModuleCreateInfo fragCreateInfo{};
    fragCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragCreateInfo.codeSize = fragBuffer.size();
    fragCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragBuffer.data());

    if (vkCreateShaderModule(device, &fragCreateInfo, nullptr, &fragModule) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vertModule, nullptr);
        throw std::runtime_error("Failed to create fragment shader module!");
    }

    auto builder = VulkanPipelineBuilder::Begin(PipelineLayout);

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = 8 * sizeof(float);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attributeDescriptions = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof(float)},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, 6 * sizeof(float)}
    };

    builder.VertexInputInfo.vertexBindingDescriptionCount = 1;
    builder.VertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    builder.VertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    builder.VertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    Pipeline = builder
        .SetShaders(vertModule, fragModule)
        .SetViewport(SwapchainExtent.width, SwapchainExtent.height)
        .SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_FILL)
        .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
        .SetMultisamplingNone()
        .EnableDepthTest(VK_TRUE, VK_COMPARE_OP_LESS)
        .SetColorBlending(true)
        .Build(device, RenderPass);

    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);

    LOG_INFO("Material Pipeline Built: {} x {}", SwapchainExtent.width, SwapchainExtent.height);
}

std::shared_ptr<FMaterialInstance> FMaterial::CreateInstance() {
    if (!Renderer) {
        LOG_ERROR("Cannot create material instance: Renderer is null");
        return nullptr;
    }

    VkDescriptorSet newSet = Renderer->AllocateDescriptorSet(DescriptorSetLayout);
    return std::make_shared<FMaterialInstance>(shared_from_this(), Renderer, AssetManager, newSet);
}
