#pragma once
#include <memory>
#include <string>
#include <vector>

namespace Tumbler {

class FActor;
class CTransform;

class FScene {
private:
    std::vector<std::unique_ptr<FActor>> Actors;
    std::vector<FActor*> PendingKillActors;

public:
    FScene();
    ~FScene();
    FScene(FScene&& other) noexcept;
    FScene& operator=(FScene&& other) noexcept;

    FScene(const FScene&) = delete;
    FScene& operator=(const FScene&) = delete;

    // ==========================================
    // 生命周期管理
    // ==========================================
    void Tick(float deltaTime);
    FActor* CreateActor(const std::string& name);
    void DestroyActor(FActor* actor);

    // ==========================================
    // 数据访问
    // ==========================================
    [[nodiscard]] const std::vector<std::unique_ptr<FActor>>& GetAllActors() const;
    [[nodiscard]] FActor* FindActorByName(const std::string& name) const;
    [[nodiscard]] bool ContainsActor(const FActor* actor) const;
};

} // namespace Tumbler
