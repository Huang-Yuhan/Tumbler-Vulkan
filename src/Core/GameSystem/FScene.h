#pragma once
#include <memory>
#include <string>
#include <vector>

#include "Components/CCamera.h"
#include "Core/Graphics/RenderPacket.h"
#include "Core/Graphics/SceneViewData.h"

class FActor;

class FScene
{
private:
    std::vector<std::unique_ptr<FActor>> Actors;
    // 【待删除队列】只存裸指针，用于帧末尾清理
    std::vector<FActor*> PendingKillActors;

public:
    FScene();
    ~FScene();
    // 移动构造函数 (Move Constructor)
    FScene(FScene&& other) noexcept;

    // 移动赋值运算符 (Move Assignment)
    FScene& operator=(FScene&& other) noexcept;

    // 禁用拷贝 (unique_ptr 不能拷贝，所以 FScene 也不能拷贝)
    FScene(const FScene&) = delete;
    FScene& operator=(const FScene&) = delete;
    // ==========================================
    // 生命周期管理
    // ==========================================

    // 逻辑更新入口 (由 MainLoop 调用)
    void Tick(float deltaTime);

    // 创建 Actor (工厂模式封装)
    FActor* CreateActor(const std::string& name);

    // 销毁 Actor (安全延迟销毁)
    void DestroyActor(FActor* actor);

    // ==========================================
    // 数据访问 (供 Renderer 使用)
    // ==========================================

    [[nodiscard]] const std::vector<std::unique_ptr<FActor>>& GetAllActors()const;

    [[nodiscard]] FActor* FindActorByName(const std::string& name) const;

    void ExtractRenderPackets(std::vector<RenderPacket>& outPackets) const;

    SceneViewData GenerateSceneView(const CCamera* camera, const CTransform* cameraTransform) const;

    // 假设场景目前存着这几个全局灯光变量（为了替换原来在 Renderer 里的硬编码）
    glm::vec3 GlobalLightPos = glm::vec3(0.0f, 4.0f, 0.0f);
    glm::vec3 GlobalLightColor = glm::vec3(1.0f, 1.0f, 1.0f);
    float GlobalLightIntensity = 50.0f;

};
