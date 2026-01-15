#pragma once
#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // 必须包含这个才能用 translate/scale
#include <Utils/FQuaternion.hpp>

class CTransform final : public Component
{
private:
    glm::vec3 Position{0.0f, 0.0f, 0.0f};
    glm::vec3 Scale{1.0f, 1.0f, 1.0f};
    FQuaternion Rotation;

    // [优化] 缓存矩阵，避免每帧重复计算
    mutable glm::mat4 CachedLocalToWorldMatrix{1.0f};
    mutable bool bIsDirty = true; // 初始设为 dirty，确保第一次获取时会计算

public:
    // ==========================================
    // Setters (必须设置 Dirty 标记)
    // ==========================================
    void SetRotation(const FQuaternion& rotation)
    {
        Rotation = rotation;
        bIsDirty = true; // 标记数据已变脏
    }

    void SetRotation(const glm::vec3& eulerDegrees)
    {
        Rotation = FQuaternion(eulerDegrees);
        bIsDirty = true;
    }

    void SetPosition(const glm::vec3& position)
    {
        Position = position;
        bIsDirty = true;
    }

    void SetScale(const glm::vec3& scale)
    {
        Scale = scale;
        bIsDirty = true;
    }

    // ==========================================
    // Getters
    // ==========================================
    glm::vec3 GetPosition() const { return Position; }
    glm::vec3 GetScale() const { return Scale; }
    FQuaternion GetRotation() const { return Rotation; }

    // 获取欧拉角通常只用于编辑器显示，不需要缓存
    glm::vec3 GetEulerAngles() const { return Rotation.ToEuler(); }

    // ==========================================
    // 核心矩阵获取 (带缓存优化)
    // ==========================================
    const glm::mat4& GetLocalToWorldMatrix() const
    {
        if (bIsDirty)
        {
            // 重新计算矩阵
            glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), Position);
            glm::mat4 rotationMatrix = Rotation.ToMatrix();
            glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), Scale);

            // T * R * S
            CachedLocalToWorldMatrix = translationMatrix * rotationMatrix * scaleMatrix;

            bIsDirty = false; // 清除脏标记
        }
        return CachedLocalToWorldMatrix;
    }

    // ==========================================
    // 辅助计算
    // ==========================================
    glm::vec3 GetForwardVector() const { return Rotation.GetForwardVector(); }
    glm::vec3 GetRightVector() const   { return Rotation.GetRightVector(); }
    glm::vec3 GetUpVector() const      { return Rotation.GetUpVector(); }

    glm::vec3 TransformDirection(const glm::vec3& localDirection) const
    {
        // Direction w=0，忽略位移
        // 注意：这里会包含 Scale 的影响。如果只想旋转，应该直接用 Rotation * localDirection
        return glm::vec3(GetLocalToWorldMatrix() * glm::vec4(localDirection, 0.0f));
    }

    glm::vec3 TransformPoint(const glm::vec3& localPoint) const
    {
        // [优化] 对于仿射变换 (Model Matrix)，底行总是 (0,0,0,1)。
        // 变换后的 w 分量永远是 1.0，所以不需要做透视除法 (/w)。
        // 透视除法只有在经过 Projection Matrix 后才需要。
        return glm::vec3(GetLocalToWorldMatrix() * glm::vec4(localPoint, 1.0f));
    }
};

//CTransform必须可以转换为Component*
static_assert(std::is_base_of_v<Component, CTransform>, "CTransform must be a subclass of Component");