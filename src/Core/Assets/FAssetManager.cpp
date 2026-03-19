#include "FAssetManager.h"
#include <Core/Graphics/VulkanRenderer.h>
#include <Core/Geometry/FMesh.h>
#include <Core/Assets/FTexture.h>
#include <Core/Assets/FMaterial.h>
#include <Core/Utils/Log.h>

void FAssetManager::Initialize(VulkanRenderer* renderer) {
    Renderer = renderer;
    LOG_INFO("FAssetManager Initialized");
}

void FAssetManager::Cleanup() {
    std::lock_guard<std::mutex> lock(AssetMutex);
    // 智能指针会自动释放内存
    // VulkanMesh 的 Buffer 会在析构时销毁，Texture 也是
    MeshCache.clear();
    TextureCache.clear();
    MaterialCache.clear();
    LOG_INFO("FAssetManager Cleaned Up");
}

std::shared_ptr<FMesh> FAssetManager::GetOrLoadMesh(const std::string& name, const std::string& filepath) {
    std::lock_guard<std::mutex> lock(AssetMutex);

    // 1. 查缓存
    auto it = MeshCache.find(name);
    if (it != MeshCache.end()) {
        return it->second;
    }

    // 2. 如果没找到，且没有提供文件路径（说明这是一个按需生成的Mesh但还没生成），返回空
    if (filepath.empty()) {
        LOG_ERROR("Mesh not found in cache and no filepath provided: {}", name);
        return nullptr;
    }

    // 3. 真正加载
    LOG_INFO("Loading new shared Mesh: {} from {}", name, filepath);
    auto mesh = std::make_shared<FMesh>(FMesh::LoadFromOBJ(filepath));
    
    // 4. 自动推送到 GPU
    if (Renderer) {
        Renderer->UploadMesh(mesh.get());
    }

    // 5. 存入缓存
    MeshCache[name] = mesh;
    return mesh;
}

void FAssetManager::RegisterMesh(const std::string& name, std::shared_ptr<FMesh> mesh) {
    std::lock_guard<std::mutex> lock(AssetMutex);
    
    // 如果缓存已有，给个警告
    if (MeshCache.find(name) != MeshCache.end()) {
        LOG_WARN("RegisterMesh: Overwriting existing mesh '{}'", name);
    }

    if (Renderer && mesh) {
        Renderer->UploadMesh(mesh.get());
    }

    MeshCache[name] = mesh;
}

std::shared_ptr<FTexture> FAssetManager::GetOrLoadTexture(const std::string& name, const std::string& filepath) {
    std::lock_guard<std::mutex> lock(AssetMutex);

    auto it = TextureCache.find(name);
    if (it != TextureCache.end()) {
        return it->second;
    }

    if (filepath.empty() || !Renderer) {
        LOG_ERROR("Texture not found and cannot be loaded: {}", name);
        return nullptr;
    }

    LOG_INFO("Loading new shared Texture: {} from {}", name, filepath);
    // 原 TextureManager 的逻辑：调用 Renderer 加载
    auto newTexture = Renderer->LoadTexture(filepath);

    if (newTexture) {
        TextureCache[name] = newTexture;
        return newTexture;
    }

    LOG_ERROR("Failed to load texture: {}", filepath);
    return nullptr;
}

std::shared_ptr<FMaterial> FAssetManager::GetOrLoadMaterial(const std::string& name, const std::string& vertPath, const std::string& fragPath) {
    std::lock_guard<std::mutex> lock(AssetMutex);

    auto it = MaterialCache.find(name);
    if (it != MaterialCache.end()) {
        return it->second;
    }

    if (vertPath.empty() || fragPath.empty() || !Renderer) {
        LOG_ERROR("Material not found and cannot be built: {}", name);
        return nullptr;
    }

    LOG_INFO("Building new Master Material: {} from {} / {}", name, vertPath, fragPath);
    auto newMaterial = std::make_shared<FMaterial>(Renderer, this);
    newMaterial->BuildPipeline(vertPath, fragPath);

    MaterialCache[name] = newMaterial;
    return newMaterial;
}
