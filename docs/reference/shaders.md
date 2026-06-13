# 着色器参考

## 文件清单

| 文件 | 类型 | 用途 |
|------|------|------|
| `assets/shaders/engine/frustum_cull.comp` | Compute | GPU Frustum Culling |
| `assets/shaders/engine/depth.vert` | Vertex | Shadow Map 深度 Pass |
| `assets/shaders/engine/gbuffer.vert` | Vertex | G-Buffer 顶点（从 SSBO 读 Transform） |
| `assets/shaders/engine/gbuffer.frag` | Fragment | G-Buffer MRT 写入 |
| `assets/shaders/engine/lighting.vert` | Vertex | 全屏三角形 |
| `assets/shaders/engine/lighting.frag` | Fragment | Deferred PBR Lighting + PCF 阴影 |

编译产物 `.spv` 由 CMake 通过 `add_subdirectory(assets/shaders)` 自动生成。

---

## 绑定规范

### Set 0 — 全局场景 UBO（所有 Shader 共享）

| Binding | 类型 | 成员 |
|---------|------|------|
| 0 | Uniform Buffer | `SceneDataUBO`（ViewProj, InvViewProj, CameraPos, LightData[8], LightCount, LightViewProj, FrustumPlanes[6]） |
| 1 | Combined Image Sampler | Shadow Map（比较采样模式） |

### Set 1 — Bindless（初始化时写入一次）

| Binding | 类型 | 说明 |
|---------|------|------|
| 0 | Combined Image Sampler[] | 纹理数组（最多 1024），所有 Albedo/Normal/MetalRoughness 纹理 |
| 1 | Storage Buffer | `MaterialData[]` SSBO（材质参数 + 纹理索引） |
| 2 | Storage Buffer | `ObjectData[]` SSBO（Transform + MeshIndex + MaterialIndex） |

### Set 1 — Bindless（仅 Culling Compute）

| Binding | 类型 | 说明 |
|---------|------|------|
| 0 | Storage Buffer | `ObjectData[]` SSBO |
| 1 | Storage Buffer | `MeshData[]` SSBO |
| 2 | Storage Buffer | `IndirectDraw[]` SSBO（写入） |
| 3 | Storage Buffer | `VisibleCount` SSBO（原子计数） |

---

## G-Buffer 布局

| Attachment | Format | 通道 |
|------------|--------|------|
| 0 | Swapchain Format（`B8G8R8A8_SRGB`） | 最终输出 |
| 1 | `R8G8B8A8_UNORM` | Albedo (RGB) |
| 2 | `R16G16B16A16_SFLOAT` | Normal (RG Octahedron), Roughness (B), Metallic (A) |
| 3 | Swapchain Depth Format | Depth（与 Forward 共享） |

---

## GPU 数据结构（GLSL 定义）

```glsl
// ObjectDataSSBO (Set 1, Binding 2)
struct ObjectGPUData {
    mat4 modelMatrix;
    uint meshIndex;
    uint materialIndex;
    uint _pad[2];
};

// MeshDataSSBO (Culling Compute)
struct MeshGPUData {
    uint64_t vertexAddress;  // 实际为 uvec2 in GLSL
    uint64_t indexAddress;
    uint indexCount;
    uint vertexCount;
    vec4 boundingSphere;     // xyz = center, w = radius
};

// MaterialDataSSBO (Set 1, Binding 1)
struct MaterialGPUData {
    uint albedoTexIndex;
    uint normalTexIndex;
    uint metallicRoughnessTexIndex;
    float roughness;
    float metallic;
    vec4 baseColorTint;      // w = TwoSided flag
};
```

## SceneDataUBO 布局（Set 0, Binding 0）

```glsl
struct LightGPUData {
    vec4 Position;   // xyz = pos (point) or direction (directional), w = type
    vec4 Color;      // rgb = color, a = intensity
    vec4 Direction;  // xyz = direction (directional), w = range (point)
};

layout(set = 0, binding = 0, std140) uniform SceneData {
    mat4 ViewProj;
    mat4 InvViewProj;
    vec4 CameraPos;
    LightGPUData Lights[8];
    int LightCount;
    int _pad[3];
    mat4 LightViewProj;
    vec4 FrustumPlanes[6];   // 视锥体 6 平面
} scene;
```
