#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>

class VulkanRenderer;
class FMesh;
class FTexture;
class FMaterial;

/**
 * @brief 全局资源管理器 (Singleton)
 * 统一管理 Mesh、Texture、Material 母体材质的生命周期。
 * 避免相同资源的重复加载，并负责将网格和纹理自动推送到 GPU。
 */
class FAssetManager {
public:
    FAssetManager() = default;
    ~FAssetManager() { Cleanup(); }

    // 初始化管理器，传入 Renderer 指针以便自动执行 GPU 上传
    void Initialize(VulkanRenderer* renderer);
    
    // 清理所有缓存。在引擎关闭时调用
    void Cleanup();

    // ==========================================
    // 1. Mesh 管理
    // ==========================================
    // 从 OBJ 加载网格，自动调用 UploadMesh 存入 GPU，并缓存共享指针
    std::shared_ptr<FMesh> GetOrLoadMesh(const std::string& name, const std::string& filepath = "");

    // 注册手动创建的网格 (如代码生成的平面)，也会自动调用 UploadMesh
    void RegisterMesh(const std::string& name, std::shared_ptr<FMesh> mesh);

    // ==========================================
    // 2. Texture 管理
    // ==========================================
    // 加载纹理，调用 Renderer 逻辑创建 VkImage 并缓存
    std::shared_ptr<FTexture> GetOrLoadTexture(const std::string& name, const std::string& filepath = "");

    // ==========================================
    // 3. Material 管理
    // ==========================================
    // 加载并构建 Shader Pipeline
    std::shared_ptr<FMaterial> GetOrLoadMaterial(const std::string& name, const std::string& vertPath, const std::string& fragPath, const std::string& deferredFragPath = "assets/shaders/engine/deferred_geometry.frag.spv");

private:
    // 禁用拷贝和赋值
    FAssetManager(const FAssetManager&) = delete;
    FAssetManager& operator=(const FAssetManager&) = delete;

    VulkanRenderer* Renderer = nullptr;

    // 资源缓存池
    std::unordered_map<std::string, std::shared_ptr<FMesh>>     MeshCache;
    std::unordered_map<std::string, std::shared_ptr<FTexture>>  TextureCache;
    std::unordered_map<std::string, std::shared_ptr<FMaterial>> MaterialCache;

    // 简单的线程安全保护
    std::mutex AssetMutex;
};
