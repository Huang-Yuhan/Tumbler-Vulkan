#pragma once
#include <memory>
#include <vector>

#include "FActor.h"

class FScene
{
private:
    std::vector<std::unique_ptr<FActor>> Actors;
    // 【待删除队列】只存裸指针，用于帧末尾清理
    std::vector<FActor*> PendingKillActors;

public:
    FScene() = default;
    ~FScene() = default;
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

    const std::vector<std::unique_ptr<FActor>>& GetAllActors() const { return Actors; }

};
