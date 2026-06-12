#pragma once

namespace Tumbler::Math {

template <typename T>
struct TVector3;

template <typename T>
struct TVector4;

template <typename T>
struct TMatrix4;

template <typename T>
struct TPlane;

struct Frustum;

using Vector3f = TVector3<float>;
using Vector4f = TVector4<float>;
using Matrix4f = TMatrix4<float>;
using Planef = TPlane<float>;

} // namespace Tumbler::Math
