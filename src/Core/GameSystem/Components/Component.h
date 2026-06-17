#pragma once

// 1. 前置声明 (正确，解耦了 FActor.h)
class FActor;

class Component {
protected:
    // [Ownership] 非拥有型指针
    FActor* Owner = nullptr;

public:
    Component();
    virtual ~Component();

    void SetOwner(FActor* InOwner);
    virtual FActor* GetOwner();

    virtual void OnDrawUI() {}
    virtual void Update(float DeltaTime);
};