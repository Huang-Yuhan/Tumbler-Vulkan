#pragma once

#include <concepts>

namespace Tumbler::Math {

// 前向声明（requires 子句与定义一致）
template <typename T> requires std::floating_point<T> struct TVector2;

template <typename T> requires std::floating_point<T> struct TVector3;

template <typename T> requires std::floating_point<T> struct TVector4;

template <typename T> requires std::floating_point<T> struct TMatrix4;

template <typename T> requires std::floating_point<T> struct TPlane;

template <typename T> requires std::floating_point<T> struct TQuaternion;

struct Frustum;

// 常用类型别名
using Vector2f = TVector2<float>;
using Vector3f = TVector3<float>;
using Vector4f = TVector4<float>;
using Matrix4f = TMatrix4<float>;
using Planef = TPlane<float>;
using Quaternionf = TQuaternion<float>;

} // namespace Tumbler::Math
