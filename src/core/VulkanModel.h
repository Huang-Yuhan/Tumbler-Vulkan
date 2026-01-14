#pragma once

#include "VulkanDevice.h"

// 引入 GLM 数学库
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <vector>

namespace Tumbler {

    class VulkanModel {
    public:
        // 定义顶点结构体 (固定格式：Pos + Color)
        struct Vertex {
            glm::vec2 position; // 对应 Shader 里的 vec2 inPosition
            glm::vec3 color;    // 对应 Shader 里的 vec3 inColor

            static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
        };

        // 构造函数
        VulkanModel(VulkanDevice &device, const std::vector<Vertex> &vertices, const std::vector<uint16_t> &indices = {});
        ~VulkanModel();

        VulkanModel(const VulkanModel &) = delete;
        VulkanModel &operator=(const VulkanModel &) = delete;

        // 绑定 Vertex Buffer 和 Index Buffer 到命令缓冲
        void bind(VkCommandBuffer commandBuffer);
        // 发起绘制命令
        void draw(VkCommandBuffer commandBuffer);

    private:
        void createVertexBuffers(const std::vector<Vertex> &vertices);
        void createIndexBuffers(const std::vector<uint16_t> &indices);

        // 内部用的辅助函数
        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory);

        VulkanDevice &vulkanDevice;

        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        uint32_t vertexCount;

        bool hasIndexBuffer = false;
        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;
        uint32_t indexCount;
    };

} // namespace Tumbler