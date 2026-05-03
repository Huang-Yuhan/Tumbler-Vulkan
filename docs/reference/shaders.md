# 着色器参考

## 文件清单

| 文件 | 类型 | 用途 |
|------|------|------|
| `assets/shaders/engine/pbr.vert` | Vertex | Forward / Deferred 共用顶点着色器 |
| `assets/shaders/engine/pbr.frag` | Fragment | Forward 渲染路径 PBR 光照 |
| `assets/shaders/engine/deferred_geometry.frag` | Fragment | Deferred 路径 G-Buffer 写入 |
| `assets/shaders/engine/deferred_lighting.vert` | Vertex | Deferred 全屏光照 triangle pass |
| `assets/shaders/engine/deferred_lighting.frag` | Fragment | Deferred 光照累积 |

编译产物 `.spv` 由 CMake 通过 `add_subdirectory(assets/shaders)` 自动生成。

---

## 绑定规范

### Set 0 — 全局场景 UBO（所有着色器共享）

| Binding | 类型 | 成员 |
|---------|------|------|
| 0 | Uniform Buffer | `SceneDataUBO`（ViewProj, InvViewProj, CameraPos, LightData[8], LightCount） |

### Set 1 — 材质描述符（pbr.frag / deferred_geometry.frag）

| Binding | 类型 | 说明 |
|---------|------|------|
| 0 | Combined Image Sampler | BaseColorMap（Albedo 贴图） |
| 1 | Combined Image Sampler | NormalMap（法线贴图） |

### Push Constants（pbr.vert / deferred_geometry.frag）

| Offset | Size | 内容 |
|--------|------|------|
| 0 | 64 bytes | `mat4` ModelMatrix |

### Deferred Lighting Set 1（deferred_lighting.frag — Input Attachment）

| Binding | 类型 | 格式 | 内容 |
|---------|------|------|------|
| 0 | Input Attachment | `R8G8B8A8_UNORM` | Albedo |
| 1 | Input Attachment | `R16G16B16A16_SFLOAT` | Normal + Roughness |
| 2 | Input Attachment | `D32_SFLOAT` | Depth（用于重建世界坐标） |

---

## G-Buffer 布局（Deferred 模式）

| Attachment | 格式 | 通道 |
|------------|------|------|
| 0 | Swapchain Format (`B8G8R8A8_SRGB`) | 最终输出 |
| 1 | `R8G8B8A8_UNORM` | Albedo (RGB) |
| 2 | `R16G16B16A16_SFLOAT` | Normal (RG), Roughness (B) |
| 3 | Swapchain Depth Format | Depth（与 Forward 共享） |

---

## 材质参数（MaterialUBO — Set 1 Binding 2）

```glsl
layout(binding = 2, std140) uniform MaterialParams {
    vec4  BaseColorTint;      // 默认 (1,1,1,1)
    float Roughness;          // 默认 0.5
    float Metallic;           // 默认 0.0
    float NormalMapStrength;  // 默认 1.0
    int   TwoSided;           // 0 = 单面, 1 = 双面
} material;
```

C++ 端对应 `FMaterialUBO` 结构体（`FMaterialInstance.h`），必须与 GLSL 的 `std140` 对齐保持一致。
