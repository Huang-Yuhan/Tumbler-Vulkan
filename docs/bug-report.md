# Bug Report: Nanite Debug Rendering — Black Screen

**Date:** 2026-07-28
**Branch:** `nanite-v2`
**Commit:** `5883e2c`

## Symptom

`vkCmdDraw` produces zero visible fragments. Clear color renders correctly, but no draw output.

Applies to: `vkCmdDraw(3, 1)` with hardcoded vertex shader (full-screen triangle, no SSBO dependency).

## Environment

- **OS:** Arch Linux, Wayland
- **GPU:** Intel(R) Graphics RPL-P (integrated)
- **Vulkan:** 1.4.350.1
- **Compiler:** GCC 16.1.1, C++26

Also reproduced on Windows (MSVC).

## What Works

- OBJ loading → Nanite partition (32 clusters, 4246 verts, 12288 indices)
- GPU buffer upload (staging → device copy)
- Vulkan instance/device/swapchain creation
- Dynamic rendering clear (verified: clear color changes are visible)
- **0 Vulkan validation errors** (all VUIDs resolved)

## What Fails

No fragments are produced by the draw call. Confirmed with:

1. **Hardcoded vertex shader** — bypasses all SSBO reads, outputs constant clip-space triangle:
   ```hlsl
   float x = (vertexID % 3 == 0) ? -1.0 : ((vertexID % 3 == 1) ? 3.0 : -1.0);
   float y = (vertexID % 3 == 0) ? -1.0 : ((vertexID % 3 == 1) ? -1.0 : 3.0);
   o.position = float4(x, y, 0.0, 1.0);
   ```

2. **Hardcoded fragment shader** — ignores input, outputs constant orange:
   ```hlsl
   return float4(1.0, 0.5, 0.0, 1.0);
   ```

3. **Minimal draw** — `vkCmdDraw(3, 1, 0, 0)` (single triangle, single instance)

Even with all SSBO reads removed and positions hardcoded, the triangle is not visible.

## What We've Ruled Out

| Hypothesis | Result |
|---|---|
| Stride mismatch (`StructuredBuffer<float3>` = 16 bytes vs `glm::vec3` = 12) | Fixed: changed to `StructuredBuffer<float>` with manual float3 construction |
| Back-face culling | Fixed: `cullMode = VK_CULL_MODE_NONE` |
| Camera too close / model off-screen | Fixed: camera at (0, 10, 80), near=0.5, far=500 |
| `slangc` not found in VS Code build | Fixed: `find_program(SLANGC_EXECUTABLE)` with vcpkg path |
| Missing Vulkan features | Fixed: `dynamicRendering`, `timelineSemaphore`, `shaderDrawParameters` |
| Descriptor set mismatch | Ruled out: 0 VUID errors, Set 0 bindings match pipeline layout |
| SPV entry point name | Verified: both shaders use "main" |
| SPV stale in build dir | Verified: SPVs match source, copied fresh |

## Current State

The vertex shader in `shaders/nanite_debug.slang` is a hardcoded full-screen triangle test. The fragment shader outputs orange. The draw call uses `vkCmdDraw(3, 1)`. No SSBO bindings are referenced by the shader at all. Yet the screen remains the clear color (purple).

### Shader (debug version)

```hlsl
// Vertex: hardcoded full-screen triangle
[shader("vertex")]
VSOutput vert_main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID) {
    float x = (vertexID % 3 == 0) ? -1.0 : ((vertexID % 3 == 1) ? 3.0 : -1.0);
    float y = (vertexID % 3 == 0) ? -1.0 : ((vertexID % 3 == 1) ? -1.0 : 3.0);
    VSOutput o;
    o.position = float4(x, y, 0.0, 1.0);
    o.color = float3(float(instanceID) / 32.0, 0.5, 0.0);
    return o;
}

// Fragment: hardcoded orange
[shader("fragment")]
float4 frag_main(VSOutput input) : SV_Target0 {
    return float4(1.0, 0.5, 0.0, 1.0);
}
```

### Draw call

```cpp
vkCmdDraw(cmd, 3, 1, 0, 0);  // 3 verts, 1 instance, firstVertex=0, firstInstance=0
```

### Pipeline state

- `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST`
- `VK_CULL_MODE_NONE`
- `VK_POLYGON_MODE_FILL`
- `VK_COMPARE_OP_GREATER` (reversed-Z)
- Dynamic viewport + scissor
- Dynamic rendering (Vulkan 1.3)
- Pipeline layout: 1 descriptor set (3 SSBO bindings, unused), 1 push constant (mat4, unused)

## Root Cause (2026-07-28)

**Depth test rejection — all fragments discarded.**

The debug vertex shader outputs `z = 0.0` in clip space. With reversed-Z:

| Value | Meaning |
|---|---|
| `depthClearValue = 0.0` | Far plane (reversed-Z) |
| `depthCompareOp = GREATER` | Nearer fragments win |
| shader `z = 0.0` → depth buffer `0.0` | Exact match with cleared far plane |

Depth test: `0.0 > 0.0` → **FALSE** on every pixel.

### Trace

```
Clip space z=0.0 → (w=1.0) → NDC z=0.0
→ Viewport [minDepth=0, maxDepth=1]: depth = 0.0
→ Compare: 0.0 > clear(0.0) = false → fragment killed
```

### Fix

Change shader z from `0.0` → `0.5` (mid-depth in reversed-Z):
```hlsl
o.position = float4(x, y, 0.5, 1.0);  // was 0.0
```

### Why it wasn't caught earlier

The "Ruled Out" table focused on SSBO stride mismatches and Vulkan feature flags — all valid concerns, but the depth test was overlooked. The bug report's own next-step #2 ("Try without depth attachment/testing") would have caught this immediately.

## Next Steps to Investigate

1. ~~Try `VK_POLYGON_MODE_LINE` to rule out rasterizer issues~~
2. ~~Try without depth attachment/testing~~ — **would have caught this**
3. Try a compute shader fill instead of graphics pipeline
4. Run under RenderDoc to capture frame state
5. ~~Verify `gl_Position.w` is not being clipped by `VK_KHR_maintenance1`~~
6. Check if GLFW Wayland backend has any incompatibility
