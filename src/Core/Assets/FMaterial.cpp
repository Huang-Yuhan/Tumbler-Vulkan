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
        for (auto& [path, pipeline] : Pipelines) {
            if (pipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(device, pipeline, nullptr);
            }
        }
        Pipelines.clear();
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
    : Pipelines(std::move(other.Pipelines))
    , PipelineLayout(other.PipelineLayout)
    , DescriptorSetLayout(other.DescriptorSetLayout)
    , RenderDeviceRef(other.RenderDeviceRef)
    , RenderPass(other.RenderPass)
    , GlobalSetLayout(other.GlobalSetLayout)
    , SwapchainExtent(other.SwapchainExtent)
    , AssetManager(other.AssetManager)
    , Renderer(other.Renderer) {
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
            for (auto& [path, pipe] : Pipelines) {
                if (pipe != VK_NULL_HANDLE) vkDestroyPipeline(device, pipe, nullptr);
            }
            Pipelines.clear();
            if (PipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, PipelineLayout, nullptr);
            if (DescriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, DescriptorSetLayout, nullptr);
        }

        Pipelines = std::move(other.Pipelines);
        PipelineLayout = other.PipelineLayout;
        DescriptorSetLayout = other.DescriptorSetLayout;
        RenderDeviceRef = other.RenderDeviceRef;
        RenderPass = other.RenderPass;
        GlobalSetLayout = other.GlobalSetLayout;
        SwapchainExtent = other.SwapchainExtent;
        AssetManager = other.AssetManager;
        Renderer = other.Renderer;

        other.Pipelines.clear();
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

void FMaterial::BuildPipelines(const std::string& forwardVert, const std::string& forwardFrag, const std::string& deferredFrag) {
    if (!RenderDeviceRef) {
        throw std::runtime_error("RenderDevice is null!");
    }

    VkDevice device = RenderDeviceRef->GetDevice();

    // 1. 设置 Descriptor Set Layout
    VkDescriptorSetLayoutBinding baseColorBinding{};
    baseColorBinding.binding = 0;
    baseColorBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    baseColorBinding.descriptorCount = 1;
    baseColorBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding normalMapBinding{};
    normalMapBinding.binding = 1;
    normalMapBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    normalMapBinding.descriptorCount = 1;
    normalMapBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 2;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bindings[] = {baseColorBinding, normalMapBinding, uboBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 3;
    layoutInfo.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &DescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }

    // 2. 建立 Pipeline Layout
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

    // 3. 通用 Shader 帮助闭包
    auto loadShaderModule = [&](const std::string& path) -> VkShaderModule {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open()) return VK_NULL_HANDLE;
        size_t size = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(size);
        file.seekg(0);
        file.read(buffer.data(), size);
        file.close();

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = buffer.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

        VkShaderModule module;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) return VK_NULL_HANDLE;
        return module;
    };

    VkShaderModule vertModule = loadShaderModule(forwardVert);
    VkShaderModule forwardFragModule = loadShaderModule(forwardFrag);
    VkShaderModule deferredFragModule = loadShaderModule(deferredFrag);
    
    if (!vertModule || !forwardFragModule || !deferredFragModule) {
        throw std::runtime_error("Failed to load one or more shaders for Material pipelines.");
    }

    // 4. 构建共有输入结构
    auto builder = VulkanPipelineBuilder::Begin(PipelineLayout);
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = 11 * sizeof(float);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attributeDescriptions = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof(float)},
        {2, 0, VK_FORMAT_R32G32B32_SFLOAT, 6 * sizeof(float)},
        {3, 0, VK_FORMAT_R32G32_SFLOAT, 9 * sizeof(float)}
    };

    builder.VertexInputInfo.vertexBindingDescriptionCount = 1;
    builder.VertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    builder.VertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    builder.VertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // 5. 编译: 前向管线 (1 颜色目标，开启 Alpha Blend)
    Pipelines[ERenderPath::Forward] = builder
        .SetShaders(vertModule, forwardFragModule)
        .SetViewport(SwapchainExtent.width, SwapchainExtent.height)
        .SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_FILL)
        .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
        .SetMultisamplingNone()
        .EnableDepthTest(VK_TRUE, VK_COMPARE_OP_LESS)
        .SetColorBlending(true, 1) // Forward: 1 Attachment blending
        .Build(device, Renderer->GetRenderPass(ERenderPath::Forward));

    // 6. 编译: 延迟几何管线 (2 颜色目标，关闭 blend 以覆盖写入原始数值)
    Pipelines[ERenderPath::Deferred] = builder
        .SetShaders(vertModule, deferredFragModule)
        // SetViewport 不需重复调用，参数已在 builder 内
        .SetColorBlending(false, 2) // Deferred MRT: 2 Attachments (Albedo, Normal), no blending
        .Build(device, Renderer->GetRenderPass(ERenderPath::Deferred));

    // Cleanup Modules
    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, forwardFragModule, nullptr);
    vkDestroyShaderModule(device, deferredFragModule, nullptr);

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
