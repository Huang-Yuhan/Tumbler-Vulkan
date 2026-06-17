#pragma once
#include "Component.h"
#include <Core/Math/Math.h>
#include <vector>

class CTransform final : public Component {
private:
    Tumbler::Math::Vector3f Position{0.0f, 0.0f, 0.0f};
    Tumbler::Math::Vector3f Scale{1.0f, 1.0f, 1.0f};
    Tumbler::Math::Quaternionf Rotation{Tumbler::Math::Quaternionf::Identity()};

    CTransform* Parent = nullptr;
    std::vector<CTransform*> Children;

    mutable Tumbler::Math::Matrix4f CachedLocalMatrix{Tumbler::Math::Matrix4f::Identity()};
    mutable Tumbler::Math::Matrix4f CachedWorldMatrix{Tumbler::Math::Matrix4f::Identity()};
    mutable bool bIsLocalDirty = true;
    mutable bool bIsWorldDirty = true;

public:
    // ==========================================
    // 层级管理 (Implemented in CTransform.cpp)
    // ==========================================
    void SetParent(CTransform* newParent, bool bStayWorldPos = true);
    CTransform* GetParent() const { return Parent; }
    const std::vector<CTransform*>& GetChildren() const { return Children; }
    void AddChild(CTransform* child);
    void RemoveChild(CTransform* child);

    void OnDrawUI() override;

    // 级联脏标记函数
    void MarkWorldDirty();

    // ==========================================
    // Setters
    // ==========================================
    void SetRotation(const Tumbler::Math::Quaternionf& rotation) {
        Rotation = rotation;
        bIsLocalDirty = true;
        MarkWorldDirty();
    }

    void SetRotation(const Tumbler::Math::Vector3f& eulerDegrees) {
        Rotation = Tumbler::Math::Quaternionf::FromEulerDegrees(eulerDegrees);
        bIsLocalDirty = true;
        MarkWorldDirty();
    }

    void SetPosition(const Tumbler::Math::Vector3f& position) {
        Position = position;
        bIsLocalDirty = true;
        MarkWorldDirty();
    }

    void SetScale(const Tumbler::Math::Vector3f& scale) {
        Scale = scale;
        bIsLocalDirty = true;
        MarkWorldDirty();
    }

    // ==========================================
    // Getters
    // ==========================================
    Tumbler::Math::Vector3f GetPosition() const { return Position; }
    Tumbler::Math::Vector3f GetScale() const { return Scale; }
    Tumbler::Math::Quaternionf GetRotation() const { return Rotation; }

    Tumbler::Math::Vector3f GetEulerAngles() const { return Rotation.ToEulerDegrees(); }

    // ==========================================
    // 核心矩阵获取
    // ==========================================
    const Tumbler::Math::Matrix4f& GetLocalMatrix() const;
    const Tumbler::Math::Matrix4f& GetLocalToWorldMatrix() const;

    // ==========================================
    // 辅助计算
    // ==========================================
    Tumbler::Math::Vector3f GetForwardVector() const { return Rotation.GetForwardVector(); }
    Tumbler::Math::Vector3f GetRightVector() const { return Rotation.GetRightVector(); }
    Tumbler::Math::Vector3f GetUpVector() const { return Rotation.GetUpVector(); }

    Tumbler::Math::Vector3f TransformDirection(const Tumbler::Math::Vector3f& localDirection) const {
        const Tumbler::Math::Vector4f result = GetLocalToWorldMatrix().TransformVector(localDirection);
        return result.XYZ();
    }

    Tumbler::Math::Vector3f TransformPoint(const Tumbler::Math::Vector3f& localPoint) const {
        const Tumbler::Math::Vector4f result = GetLocalToWorldMatrix().TransformPosition(localPoint);
        return result.XYZ();
    }
};

static_assert(std::is_base_of_v<Component, CTransform>, "CTransform must be a subclass of Component");
