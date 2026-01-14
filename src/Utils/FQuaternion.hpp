#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

// 封装 GLM 四元数，提供更符合直觉的游戏开发接口
struct FQuaternion
{
    // 内部数据：GLM 使用 (w, x, y, z) 或 (x, y, z, w) 取决于版本，我们直接存这个对象
    glm::quat Raw;

    // ==========================================
    // 构造函数
    // ==========================================

    // 默认构造：单位四元数 (无旋转)
    FQuaternion() : Raw(glm::identity<glm::quat>()) {}

    // 直接从 glm::quat 构造
    FQuaternion(const glm::quat& InQuat) : Raw(InQuat) {}

    // 从欧拉角构造 (输入单位：角度 Degrees)
    // Order: Y -> X -> Z (常见的游戏旋转顺序: Yaw -> Pitch -> Roll)
    FQuaternion(const glm::vec3& EulerDegrees)
    {
        glm::vec3 Radians = glm::radians(EulerDegrees);
        Raw = glm::quat(Radians);
    }

    // 从轴角构造 (例如：绕 Y 轴旋转 90 度)
    static FQuaternion FromAxisAngle(const glm::vec3& Axis, float AngleDegrees)
    {
        return FQuaternion(glm::angleAxis(glm::radians(AngleDegrees), glm::normalize(Axis)));
    }

    // ==========================================
    // 核心功能
    // ==========================================

    // 获取对应的旋转矩阵 (用于构建 Model Matrix)
    glm::mat4 ToMatrix() const
    {
        return glm::toMat4(Raw);
    }

    // 转回欧拉角 (单位：角度 Degrees)
    // 注意：转换回欧拉角可能会遇到万向节死锁的奇异点，结果可能与输入不完全一致，但旋转效果是一样的
    glm::vec3 ToEuler() const
    {
        return glm::degrees(glm::eulerAngles(Raw));
    }

    // 归一化 (防止多次旋转累积误差导致变形)
    void Normalize()
    {
        Raw = glm::normalize(Raw);
    }

    // 球面插值 (Slerp) - 用于平滑动画
    static FQuaternion Slerp(const FQuaternion& A, const FQuaternion& B, float Alpha)
    {
        return FQuaternion(glm::mix(A.Raw, B.Raw, Alpha));
    }

    // ==========================================
    // 方向向量获取 (非常重要！)
    // ==========================================

    // 假设你的世界坐标系：Y Up, Z Forward (或者是 -Z Forward，根据你的 Vulkan 投影矩阵决定)
    // 这里假设标准 GLM/OpenGL 惯例：原始 Forward 是 (0,0,1) 或 (0,0,-1)
    // 你可以根据你的引擎习惯修改这里的默认向量

    glm::vec3 GetForwardVector() const
    {
        // 将 "世界前方向" (0,0,1) 施加当前旋转
        return Raw * glm::vec3(0.0f, 0.0f, 1.0f);
    }

    glm::vec3 GetRightVector() const
    {
        // 将 "世界右方向" (1,0,0) 施加当前旋转
        return Raw * glm::vec3(1.0f, 0.0f, 0.0f);
    }

    glm::vec3 GetUpVector() const
    {
        // 将 "世界顶方向" (0,1,0) 施加当前旋转
        return Raw * glm::vec3(0.0f, 1.0f, 0.0f);
    }

    // ==========================================
    // 运算符重载
    // ==========================================

    // 两个旋转叠加 (Q3 = Q2 * Q1，先转 Q1 再转 Q2)
    FQuaternion operator*(const FQuaternion& Other) const
    {
        return FQuaternion(Raw * Other.Raw);
    }

    // 旋转一个向量 (v_new = q * v)
    glm::vec3 operator*(const glm::vec3& Vec) const
    {
        return Raw * Vec;
    }
};