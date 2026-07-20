# Frustum Extraction

Tumbler exposes frustum math through `Core/Math/Math.h`.

```cpp
#include "Core/Math/Math.h"

using namespace Tumbler::Math;

const Matrix4f view = MakeLookAt(cameraPosition, cameraTarget, Vector3f::UnitY());
const Matrix4f projection = MakePerspective(
    DegreesToRadians(90.0f),
    aspectRatio,
    nearZ,
    farZ);

Frustum frustum;
if (ExtractFrustumPlanes(projection * view, frustum)) {
    const FrustumIntersection result = frustum.TestSphere(worldCenter, worldRadius);
}
```

## Plane Contract

`Planef` stores planes as `dot(normal, point) + D >= 0`, where the non-negative side is inside the frustum.
All extracted planes are normalized, so `SignedDistance()` returns world-space distance.

`Frustum` always stores planes in this order:

1. `FrustumPlane::Left`
2. `FrustumPlane::Right`
3. `FrustumPlane::Bottom`
4. `FrustumPlane::Top`
5. `FrustumPlane::Near`
6. `FrustumPlane::Far`

Callers may depend on this order.

## Depth Convention

Depth convention is shared by C++ and future HLSL through `Core/Math/MathConfig.h`.

```cpp
enum class DepthConvention {
    VulkanZeroToOne,
    ReverseZZeroToOne,
};
```

The default is `kDefaultDepthConvention`, currently `DepthConvention::VulkanZeroToOne`.
Functions such as `MakePerspective()` and `ExtractFrustumPlanes()` use that default unless a convention is passed explicitly.

The shared header also defines preprocessor constants for shader code:

```c
TUMBLER_DEPTH_CONVENTION_VULKAN_ZERO_TO_ONE
TUMBLER_DEPTH_CONVENTION_REVERSE_Z_ZERO_TO_ONE
TUMBLER_MATH_DEFAULT_DEPTH_CONVENTION
```

The project does not use runtime mutable global depth state. Switching to reverse-Z should be done as a coordinated rendering change:

- change the shared default convention;
- update projection/frustum tests;
- update Vulkan depth clear and compare state;
- update shader depth reconstruction.

## Extraction Formula

Tumbler uses row-major `Matrix4f` storage and column-vector transforms:

```cpp
clipPosition = viewProjection * worldPosition;
```

For Vulkan/D3D `0..1` depth, clip-space planes are:

| Plane | Clip-space condition | World-space row combination |
|---|---|---|
| Left | `x + w >= 0` | `row3 + row0` |
| Right | `-x + w >= 0` | `row3 - row0` |
| Bottom | `y + w >= 0` | `row3 + row1` |
| Top | `-y + w >= 0` | `row3 - row1` |
| Near | `z >= 0` | `row2` |
| Far | `-z + w >= 0` | `row3 - row2` |

For reverse-Z `0..1`, near and far swap their depth conditions:

| Plane | Clip-space condition | World-space row combination |
|---|---|---|
| Near | `-z + w >= 0` | `row3 - row2` |
| Far | `z >= 0` | `row2` |

Each plane is normalized before it is stored. If any plane cannot be normalized, `ExtractFrustumPlanes()` returns `false`.

