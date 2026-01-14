#pragma once

#include <GameSystem/FActor.h>

class Component
{

protected:
    // [Ownership] 非拥有型指针 (Non-owning pointer)。
    // 1. 避免循环引用：Component 由 Actor 通过 unique_ptr 强持有，若此处使用 shared_ptr 会导致内存泄漏。
    // 2. 生命周期安全：Component 的生命周期完全依赖于 Actor，Actor 析构时会自动销毁 Component，
    //    因此在 Component 存活期间，Owner 指针永远有效，无需使用 weak_ptr。
    FActor* Owner = nullptr;
public:
    virtual ~Component()=default;
    void SetOwner(FActor* InOwner) { Owner = InOwner; }

    virtual FActor* GetOwner() { return Owner; }
    virtual void Update(float DeltaTime){};

};


