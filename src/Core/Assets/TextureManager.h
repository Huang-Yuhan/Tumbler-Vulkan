#pragma once

#include "FTexture.h"
#include <unordered_map>
#include <string>
#include <memory>

// 前置声明，避免头文件循环依赖
class VulkanRenderer;

class TextureManager {
public:
    // Manager 需要持有 Renderer 的指针，因为加载纹理需要用到 Renderer 里的
    // ImmediateSubmit, CreateBuffer 等辅助函数
    TextureManager(VulkanRenderer* renderer);
    ~TextureManager();

    // 核心接口：给路径，还你纹理
    // 使用 shared_ptr，因为纹理可能被多个物体共享
    std::shared_ptr<FTexture> GetTexture(const std::string& filename);

    // 清空所有缓存
    void Cleanup();

private:
    VulkanRenderer* Renderer;

    // 缓存池：路径 -> 纹理指针
    std::unordered_map<std::string, std::shared_ptr<FTexture>> TextureCache;
};