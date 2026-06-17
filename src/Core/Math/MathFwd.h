#pragma once

namespace Tumbler::Math {

// 前向声明
template <typename T> struct TVector2;

template <typename T> struct TVector3;

template <typename T> struct TVector4;

template <typename T> struct TMatrix4;

template <typename T> struct TPlane;

template <typename T> struct TQuaternion;

struct Frustum;

// 常用类型别名
using Vector2f = TVector2<float>;
using Vector3f = TVector3<float>;
using Vector4f = TVector4<float>;
using Matrix4f = TMatrix4<float>;
using Planef = TPlane<float>;
using Quaternionf = TQuaternion<float>;

} // namespace Tumbler::Math
