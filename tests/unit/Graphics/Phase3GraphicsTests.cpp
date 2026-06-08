#include "Core/Graphics/CommandManager.h"
#include "Core/Graphics/DescriptorManager.h"
#include "Core/Graphics/RenderDevice.h"
#include "Core/Graphics/ResourceManager.h"
#include "Core/Graphics/VulkanContext.h"

#include <gtest/gtest.h>
#include <vulkan/vulkan.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace {

std::optional<std::string> DetectPhase3VulkanSkipReason() {
    uint32_t loaderVersion = 0;
    VkResult versionResult = vkEnumerateInstanceVersion(&loaderVersion);
    if (versionResult != VK_SUCCESS) {
        return "Vulkan loader is unavailable";
    }
    if (loaderVersion < VK_API_VERSION_1_4) {
        return "Vulkan loader does not support Vulkan 1.4";
    }

    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

    bool hasValidationLayer = false;
    for (const auto& layer : layers) {
        if (std::string(layer.layerName) == "VK_LAYER_KHRONOS_validation") {
            hasValidationLayer = true;
            break;
        }
    }
    if (!hasValidationLayer) {
        return "VK_LAYER_KHRONOS_validation is unavailable";
    }

    const char* validationLayer = "VK_LAYER_KHRONOS_validation";
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "TumblerPhase3Tests";
    appInfo.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledLayerCount = 1;
    instanceInfo.ppEnabledLayerNames = &validationLayer;

    VkInstance instance = VK_NULL_HANDLE;
    VkResult instanceResult = vkCreateInstance(&instanceInfo, nullptr, &instance);
    if (instanceResult != VK_SUCCESS) {
        return "Vulkan instance creation failed";
    }

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    bool hasSuitableDevice = false;
    for (auto device : devices) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queues(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queues.data());

        bool hasGraphicsQueue = false;
        for (const auto& queue : queues) {
            if (queue.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                hasGraphicsQueue = true;
                break;
            }
        }
        if (!hasGraphicsQueue) {
            continue;
        }

        VkPhysicalDeviceVulkan12Features features12{};
        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

        VkPhysicalDeviceFeatures2 features2{};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &features12;
        vkGetPhysicalDeviceFeatures2(device, &features2);

        if (features12.bufferDeviceAddress && features12.descriptorIndexing && features12.drawIndirectCount &&
            features12.shaderSampledImageArrayNonUniformIndexing &&
            features12.descriptorBindingSampledImageUpdateAfterBind && features12.descriptorBindingPartiallyBound &&
            features12.runtimeDescriptorArray) {
            hasSuitableDevice = true;
            break;
        }
    }

    vkDestroyInstance(instance, nullptr);

    if (!hasSuitableDevice) {
        return "No Vulkan device supports Phase 3 GPU-driven descriptor features";
    }
    return std::nullopt;
}

void SkipIfPhase3VulkanUnavailable() {
    static const std::optional<std::string> reason = DetectPhase3VulkanSkipReason();
    if (reason.has_value()) {
        GTEST_SKIP() << *reason;
    }
}

std::filesystem::path SourcePath(const char* relativePath) {
    return std::filesystem::path(TUMBLER_SOURCE_DIR) / relativePath;
}

class VulkanPhase3Fixture : public ::testing::Test {
protected:
    void SetUp() override {
        SkipIfPhase3VulkanUnavailable();
        ASSERT_TRUE(Context.Init());
        ASSERT_TRUE(Device.Init(Context.GetInstance(), Context.GetDevice(), Context.GetPhysicalDevice()));
        ASSERT_TRUE(Commands.Init(Context.GetDevice(), Context.GetGraphicsQueueFamily()));
    }

    void TearDown() override {
        if (Context.GetDevice() != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(Context.GetDevice());
        }
        Commands.Shutdown();
        Device.Shutdown();
        Context.Shutdown();
    }

    Tumbler::VulkanContext Context;
    Tumbler::RenderDevice Device;
    Tumbler::CommandManager Commands;
};

VkImageView CreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectMask) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectMask;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView view = VK_NULL_HANDLE;
    EXPECT_EQ(vkCreateImageView(device, &viewInfo, nullptr, &view), VK_SUCCESS);
    return view;
}

} // namespace

TEST(VulkanContextPhase3, InitializesRequiredHandles) {
    SkipIfPhase3VulkanUnavailable();

    Tumbler::VulkanContext context;
    ASSERT_TRUE(context.Init());

    EXPECT_NE(context.GetInstance(), VK_NULL_HANDLE);
    EXPECT_NE(context.GetPhysicalDevice(), VK_NULL_HANDLE);
    EXPECT_NE(context.GetDevice(), VK_NULL_HANDLE);
    EXPECT_NE(context.GetGraphicsQueue(), VK_NULL_HANDLE);

    context.Shutdown();
}

TEST_F(VulkanPhase3Fixture, RenderDeviceCreatesAndDestroysBuffers) {
    auto buffer = Device.CreateBuffer(256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    EXPECT_NE(buffer.Buffer, VK_NULL_HANDLE);
    EXPECT_NE(buffer.Allocation, VK_NULL_HANDLE);
    EXPECT_EQ(buffer.Address, 0);

    Device.DestroyBuffer(buffer);
    EXPECT_EQ(buffer.Buffer, VK_NULL_HANDLE);
    EXPECT_EQ(buffer.Allocation, VK_NULL_HANDLE);
    EXPECT_EQ(buffer.Address, 0);

    VkBufferCreateInfo addressBufferInfo{};
    addressBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    addressBufferInfo.size = 256;
    addressBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VmaAllocationCreateInfo addressAllocInfo{};
    addressAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    auto addressBuffer = Device.CreateBuffer(addressBufferInfo, addressAllocInfo, "TestAddressBuffer");
    EXPECT_NE(addressBuffer.Buffer, VK_NULL_HANDLE);
    EXPECT_NE(addressBuffer.Allocation, VK_NULL_HANDLE);
    EXPECT_NE(addressBuffer.Address, 0);

    Device.DestroyBuffer(addressBuffer);
    EXPECT_EQ(addressBuffer.Buffer, VK_NULL_HANDLE);
    EXPECT_EQ(addressBuffer.Allocation, VK_NULL_HANDLE);
    EXPECT_EQ(addressBuffer.Address, 0);
}

TEST_F(VulkanPhase3Fixture, RenderDeviceCreatesAndDestroysImageAndSampler) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent = {1, 1, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    VmaAllocationCreateInfo imageAllocInfo{};
    imageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    auto image = Device.CreateImage(imageInfo, imageAllocInfo);
    EXPECT_NE(image.Image, VK_NULL_HANDLE);
    EXPECT_NE(image.Allocation, VK_NULL_HANDLE);

    Device.DestroyImage(image);
    EXPECT_EQ(image.Image, VK_NULL_HANDLE);
    EXPECT_EQ(image.Allocation, VK_NULL_HANDLE);

    VkSampler sampler = Device.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 1);
    EXPECT_NE(sampler, VK_NULL_HANDLE);
    Device.DestroySampler(sampler);
}

TEST_F(VulkanPhase3Fixture, CommandManagerImmediateSubmitExecutesCommands) {
    bool called = false;
    Commands.ImmediateSubmit([&](VkCommandBuffer cmd) {
        EXPECT_NE(cmd, VK_NULL_HANDLE);
        called = true;
    });

    EXPECT_TRUE(called);
}

TEST_F(VulkanPhase3Fixture, CommandManagerTransitionsImageLayouts) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent = {1, 1, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    VmaAllocationCreateInfo imageAllocInfo{};
    imageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    auto image = Device.CreateImage(imageInfo, imageAllocInfo);
    ASSERT_NE(image.Image, VK_NULL_HANDLE);

    Commands.ImmediateSubmit([&](VkCommandBuffer cmd) {
        Commands.TransitionImageLayout(cmd, image.Image, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        Commands.TransitionImageLayout(cmd, image.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });

    Device.DestroyImage(image);
}

TEST_F(VulkanPhase3Fixture, DescriptorManagerAllocatesAndUpdatesPhase3Sets) {
    Tumbler::DescriptorManager descriptors;
    ASSERT_TRUE(descriptors.Init(Context.GetDevice(), 8));
    EXPECT_NE(descriptors.GetSet0Layout(), VK_NULL_HANDLE);
    EXPECT_NE(descriptors.GetSet1Layout(), VK_NULL_HANDLE);

    auto sceneUBO = Device.CreateBuffer(256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    auto materialSSBO = Device.CreateBuffer(256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    auto objectSSBO = Device.CreateBuffer(256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent = {1, 1, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    VmaAllocationCreateInfo imageAllocInfo{};
    imageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    auto image = Device.CreateImage(imageInfo, imageAllocInfo);
    ASSERT_NE(image.Image, VK_NULL_HANDLE);
    Commands.ImmediateSubmit([&](VkCommandBuffer cmd) {
        Commands.TransitionImageLayout(cmd, image.Image, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });

    VkImageView imageView = CreateImageView(Context.GetDevice(), image.Image, VK_FORMAT_R8G8B8A8_UNORM,
                                            VK_IMAGE_ASPECT_COLOR_BIT);
    ASSERT_NE(imageView, VK_NULL_HANDLE);
    VkSampler sampler = Device.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 1);

    VkDescriptorSet set0 = descriptors.AllocateSet(descriptors.GetSet0Layout());
    VkDescriptorSet set1 = descriptors.AllocateSet(descriptors.GetSet1Layout());
    EXPECT_NE(set0, VK_NULL_HANDLE);
    EXPECT_NE(set1, VK_NULL_HANDLE);

    descriptors.UpdateSet0(set0, sceneUBO.Buffer, imageView, sampler);
    descriptors.UpdateSet1Bindless(set1, std::vector<VkImageView>{imageView}, materialSSBO.Buffer, objectSSBO.Buffer);

    descriptors.FreeSet(set1);
    descriptors.FreeSet(set0);
    descriptors.Shutdown();

    Device.DestroySampler(sampler);
    vkDestroyImageView(Context.GetDevice(), imageView, nullptr);
    Device.DestroyImage(image);
    Device.DestroyBuffer(objectSSBO);
    Device.DestroyBuffer(materialSSBO);
    Device.DestroyBuffer(sceneUBO);
}

TEST_F(VulkanPhase3Fixture, ResourceManagerUploadsMeshTextureAndShader) {
    Tumbler::ResourceManager resources;
    ASSERT_TRUE(resources.Init(Context.GetDevice(), Device, Commands));

    auto mesh = resources.UploadMesh(SourcePath("assets/models/Sting-Sword-lowpoly.obj").string());
    EXPECT_GT(mesh.VertexCount, 0u);
    EXPECT_GT(mesh.IndexCount, 0u);
    EXPECT_GT(mesh.BoundingSphere.w, 0.0f);

    auto texture = resources.UploadTexture(SourcePath("assets/textures/white.png").string());
    EXPECT_NE(texture.Image, VK_NULL_HANDLE);
    EXPECT_NE(texture.ImageView, VK_NULL_HANDLE);
    EXPECT_EQ(texture.TextureIndex, 0u);

    auto duplicateTexture = resources.UploadTexture(SourcePath("assets/textures/white.png").string());
    EXPECT_EQ(duplicateTexture.TextureIndex, texture.TextureIndex);
    EXPECT_EQ(resources.GetTextureCount(), 1u);

    auto shaderPath = SourcePath("assets/shaders/engine/deferred_lighting.vert.spv");
    if (!std::filesystem::exists(shaderPath)) {
        resources.Shutdown();
        GTEST_SKIP() << "Shader artifact is unavailable: " << shaderPath.string();
    }

    VkShaderModule shader = resources.LoadShader(shaderPath.string());
    EXPECT_NE(shader, VK_NULL_HANDLE);
    resources.DestroyShaderModule(shader);

    resources.Shutdown();
}
