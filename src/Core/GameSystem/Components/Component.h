#pragma once

// 1. 前置声明 (正确，解耦了 FActor.h)
class FActor;

class Component
{
protected:
    // [Ownership] 非拥有型指针
    FActor* Owner = nullptr;

public:
    Component();
    virtual ~Component(); // 析构函数必须是 virtual

    // 2. 只有声明，去掉花括号 {}
    void SetOwner(FActor* InOwner);
    virtual FActor* GetOwner();

    // Update 也可以放进 cpp，保持整洁
    virtual void Update(float DeltaTime);
};