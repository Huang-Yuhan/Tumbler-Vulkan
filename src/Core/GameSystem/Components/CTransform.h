#pragma once
#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // 必须包含这个才能用 translate/scale
#include <Core/Utils/FQuaternion.hpp>

class CTransform final : public Component
{
private:
    glm::vec3 Position{0.0f, 0.0f, 0.0f};
    glm::vec3 Scale{1.0f, 1.0f, 1.0f};
    FQuaternion Rotation;

    CTransform* Parent = nullptr;
    std::vector<CTransform*> Children;

    mutable glm::mat4 CachedLocalMatrix{1.0f};
    mutable glm::mat4 CachedWorldMatrix{1.0f};
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

    // 级联脏标记函数
    void MarkWorldDirty();

    // ==========================================
    // Setters (设置自身 Dirty 及级联子节点 Dirty)
    // ==========================================
    void SetRotation(const FQuaternion& rotation)
    {
        Rotation = rotation;
        bIsLocalDirty = true;
        MarkWorldDirty();
    }

    void SetRotation(const glm::vec3& eulerDegrees)
    {
        Rotation = FQuaternion(eulerDegrees);
        bIsLocalDirty = true;
        MarkWorldDirty();
    }

    void SetPosition(const glm::vec3& position)
    {
        Position = position;
        bIsLocalDirty = true;
        MarkWorldDirty();
    }

    void SetScale(const glm::vec3& scale)
    {
        Scale = scale;
        bIsLocalDirty = true;
        MarkWorldDirty();
    }

    // ==========================================
    // Getters
    // ==========================================
    glm::vec3 GetPosition() const { return Position; }
    glm::vec3 GetScale() const { return Scale; }
    FQuaternion GetRotation() const { return Rotation; }

    glm::vec3 GetEulerAngles() const { return Rotation.ToEuler(); }

    // ==========================================
    // 核心矩阵获取 (Implemented in CTransform.cpp)
    // ==========================================
    const glm::mat4& GetLocalMatrix() const;
    const glm::mat4& GetLocalToWorldMatrix() const;

    // ==========================================
    // 辅助计算
    // ==========================================
    glm::vec3 GetForwardVector() const { return Rotation.GetForwardVector(); }
    glm::vec3 GetRightVector() const   { return Rotation.GetRightVector(); }
    glm::vec3 GetUpVector() const      { return Rotation.GetUpVector(); }

    glm::vec3 TransformDirection(const glm::vec3& localDirection) const
    {
        return glm::vec3(GetLocalToWorldMatrix() * glm::vec4(localDirection, 0.0f));
    }

    glm::vec3 TransformPoint(const glm::vec3& localPoint) const
    {
        return glm::vec3(GetLocalToWorldMatrix() * glm::vec4(localPoint, 1.0f));
    }
};

//CTransform必须可以转换为Component*
static_assert(std::is_base_of_v<Component, CTransform>, "CTransform must be a subclass of Component");