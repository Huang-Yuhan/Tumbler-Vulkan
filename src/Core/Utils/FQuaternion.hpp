#pragma once

#include <glm/glm.hpp>
// 只使用稳定版 GTC 扩展
#include <glm/gtc/quaternion.hpp>
// 包含 matrix_transform 以防万一需要基础矩阵运算
#include <glm/gtc/matrix_transform.hpp>

struct FQuaternion
{
    glm::quat Raw;

    // ==========================================
    // 构造函数
    // ==========================================

    // 1. 默认构造：单位四元数
    FQuaternion() : Raw(glm::identity<glm::quat>()) {}

    // 2. 从 glm::quat 构造
    FQuaternion(const glm::quat& InQuat) : Raw(InQuat) {}

    // 3. 从欧拉角构造 (输入单位：角度 Degrees)
    // GLM 标准构造函数 glm::quat(vec3) 期望的是弧度 (Pitch, Yaw, Roll)
    FQuaternion(const glm::vec3& EulerDegrees)
    {
        glm::vec3 Radians = glm::radians(EulerDegrees);
        // 使用 GTC 标准构造，通常顺序是 pitch(x), yaw(y), roll(z)
        Raw = glm::quat(Radians);
    }

    // 4. 从轴角构造
    static FQuaternion FromAxisAngle(const glm::vec3& Axis, float AngleDegrees)
    {
        // glm::angleAxis 是 GTC 的一部分，安全
        return FQuaternion(glm::angleAxis(glm::radians(AngleDegrees), glm::normalize(Axis)));
    }

    // ==========================================
    // 核心功能
    // ==========================================

    // [关键修改] 获取旋转矩阵
    // 以前是 glm::toMat4 (GTX)，现在改用 glm::mat4_cast (GTC 标准)
    glm::mat4 ToMatrix() const
    {
        return glm::mat4_cast(Raw);
    }

    // 转回欧拉角 (单位：角度 Degrees)
    glm::vec3 ToEuler() const
    {
        // glm::eulerAngles 是 GTC 的一部分，返回 (Pitch, Yaw, Roll)
        return glm::degrees(glm::eulerAngles(Raw));
    }

    // 归一化
    void Normalize()
    {
        Raw = glm::normalize(Raw);
    }

    // 球面插值 (Slerp)
    static FQuaternion Slerp(const FQuaternion& A, const FQuaternion& B, float Alpha)
    {
        // glm::slerp 是 GTC 的标准函数 (以前你用的 mix 也可以，但 slerp 语义更明确)
        return FQuaternion(glm::slerp(A.Raw, B.Raw, Alpha));
    }

    // ==========================================
    // 方向向量获取
    // ==========================================

    // 提示：GLM 的四元数 * 向量 (Raw * vec3) 也是 GTC 标准操作，不需要 GTX

    glm::vec3 GetForwardVector() const
    {
        // 假设 Vulkan/OpenGL 默认前向是 Z+ 或 Z-，根据你的坐标系调整
        // 常见是 (0, 0, 1)
        return Raw * glm::vec3(0.0f, 0.0f, 1.0f);
    }

    glm::vec3 GetRightVector() const
    {
        return Raw * glm::vec3(1.0f, 0.0f, 0.0f);
    }

    glm::vec3 GetUpVector() const
    {
        return Raw * glm::vec3(0.0f, 1.0f, 0.0f);
    }

    // ==========================================
    // 运算符重载
    // ==========================================

    FQuaternion operator*(const FQuaternion& Other) const
    {
        return FQuaternion(Raw * Other.Raw);
    }

    glm::vec3 operator*(const glm::vec3& Vec) const
    {
        return Raw * Vec;
    }
};