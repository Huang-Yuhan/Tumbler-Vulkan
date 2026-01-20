#include "TextureManager.h"
#include <Core/Graphics/VulkanRenderer.h>

TextureManager::TextureManager(VulkanRenderer* renderer) : Renderer(renderer) {
}

TextureManager::~TextureManager() {
    Cleanup();
}

std::shared_ptr<FTexture> TextureManager::GetTexture(const std::string& filename) {
    // 1. 查缓存：如果已经在 Map 里，直接返回
    auto it = TextureCache.find(filename);
    if (it != TextureCache.end()) {
        return it->second;
    }

    // 2. 没找到：调用 Renderer 去真正加载
    // 注意：我们需要修改 Renderer::LoadTexture 让它返回 shared_ptr，或者在这里转换
    std::shared_ptr<FTexture> newTexture = Renderer->LoadTexture(filename);

    if (newTexture) {
        // 3. 存入缓存
        TextureCache[filename] = newTexture;
        return newTexture;
    }

    return nullptr;
}

void TextureManager::Cleanup() {
    // shared_ptr 会在 map 清空且没人引用时自动析构 FTexture
    // FTexture 析构函数会自动释放显存
    TextureCache.clear();
}