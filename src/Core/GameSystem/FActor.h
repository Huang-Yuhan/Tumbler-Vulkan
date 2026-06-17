#pragma once

#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "Core/GameSystem/Components/CTransform.h"
#include "Core/GameSystem/Components/Component.h"

namespace Tumbler {

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
    std::unordered_map<std::type_index, Component*> TypeMap;

    static FActor* CreateActor(const std::string& name);

    template <typename T, typename... Args> T* AddComponent(Args&&... args) {
        auto NewComp = std::make_unique<T>(std::forward<Args>(args)...);
        NewComp->SetOwner(this);
        T* Ptr = NewComp.get();
        TypeMap[std::type_index(typeid(T))] = Ptr;
        Components.push_back(std::move(NewComp));
        return Ptr;
    }

    template <typename T> T* GetComponent() {
        auto it = TypeMap.find(std::type_index(typeid(T)));
        if (it != TypeMap.end()) {
            return static_cast<T*>(it->second);
        }
        // Fallback for types added before TypeMap existed
        for (auto& Comp : Components) {
            if (T* Ptr = dynamic_cast<T*>(Comp.get())) {
                return Ptr;
            }
        }
        return nullptr;
    }

    // 获取多个同类型组件
    template <typename T> std::vector<T*> GetComponents() {
        std::vector<T*> results;
        for (auto& Comp : Components) {
            if (T* Ptr = dynamic_cast<T*>(Comp.get())) {
                results.push_back(Ptr);
            }
        }
        return results;
    }
};

template <> inline CTransform* FActor::GetComponent<CTransform>() {
    // 直接返回成员变量的地址
    return &(this->Transform);
}

} // namespace Tumbler
