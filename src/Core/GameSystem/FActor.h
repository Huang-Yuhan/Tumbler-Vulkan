#pragma once

#include <string>
#include <vector>
#include <memory>

// 因为 Transform 是成员变量(值类型)，必须包含头文件，不能前置声明
#include "Core/GameSystem/Components/CTransform.h"
// 因为 Template 函数里调用了 Comp->SetOwner，编译器需要看到 Component 的完整定义
#include "Core/GameSystem/Components/Component.h"

class FActor {
private:
    // 构造函数私有化，强制使用 CreateActor
    FActor();

public:
    // 析构函数 (在 cpp 里实现)
    ~FActor();

    std::string Name;
    CTransform Transform; // 99% 的物体都有，做成成员变量没问题

    // 存储所有组件
    std::vector<std::unique_ptr<Component>> Components;

    // ==========================================
    // 静态工厂方法 (声明)
    // ==========================================
    static FActor* CreateActor(const std::string& name);

    // ==========================================
    // 模板函数 (实现必须保留在 .h 文件中)
    // ==========================================

    // 添加组件
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args) {
        // 1. 创建组件
        auto NewComp = std::make_unique<T>(std::forward<Args>(args)...);

        // 2. 设置 Owner
        NewComp->SetOwner(this);

        // 3. 获取原始指针用于返回
        T* Ptr = NewComp.get();

        // 4. 移交所有权给 vector
        Components.push_back(std::move(NewComp));

        return Ptr;
    }

    // 获取单个组件
    template<typename T>
    T* GetComponent() {
        for (auto& Comp : Components) {
            if (T* Ptr = dynamic_cast<T*>(Comp.get())) {
                return Ptr;
            }
        }
        return nullptr;
    }

    // 获取多个同类型组件
    template<typename T>
    std::vector<T*> GetComponents() {
        std::vector<T*> results;
        for (auto& Comp : Components) {
            if (T* Ptr = dynamic_cast<T*>(Comp.get())) {
                results.push_back(Ptr);
            }
        }
        return results;
    }
};


template<>
inline CTransform * FActor::GetComponent<CTransform>()
{
    // 直接返回成员变量的地址
    return &(this->Transform);
}
