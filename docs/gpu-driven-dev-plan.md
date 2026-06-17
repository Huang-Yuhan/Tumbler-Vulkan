# Nanite-Like GPU-Driven 渲染器开发计划

## 概述

在 `nanite-integration` 分支上开发。目标：复现 UE5 Nanite 的核心管线——**Visibility Buffer + Cluster 层级 + SW/HW 混合光栅 + Deferred Material**。

参考引擎：`C:\UnrealEngine\Engine\Source\Runtime\Renderer\Private\Nanite\`

---

## 一、架构总览

### 每帧 GPU 管线

```
Compute  │ 1. InstanceCull       → 剔除不可见实例
Compute  │ 2. NodeCull            → BVH 层级遍历 + 屏幕误差 LOD 选择（Phase 6+）
Compute  │ 3. ClusterCull         → 逐 Cluster 视锥体 + HZB 遮挡（Phase 4: 仅视锥体）
Compute  │ 4. RasterBinBuild      → 按材质分箱（HW bin / SW bin，Phase 8+）
Compute  │ 5. SW Raster            → 小三角形 Compute 光栅 → 原子写 VisBuffer64
Graphics │ 6. HW Raster (Indirect) → 大三角形硬件光栅 → 原子写 VisBuffer64（Phase 8+）
Compute  │ 7. DepthExport          → VisBuffer64 解码深度 → SceneDepth + ShadingMask
Compute  │ 8. ShadeBinning         → 按材质桶排序像素
Compute  │ 9. ShadeGBuffer         → 解码 VisBuffer → 材质求值 → 写 G-Buffer UAV
Graphics │ 10. Lighting             → 标准 Deferred Lighting（全屏三角形）
Graphics │ 11. ImGui                → 编辑器叠加
```

### 数据流

```
Mesh 资产（OBJ/GLTF）
  │  CPU 预处理（Phase 4B: 仅 LOD 0）
  ▼
Cluster 数组（单层级，LOD 0）
  │  Upload
  ▼
GPU Buffer: ClusterPageData
  │
  ▼
[ClusterCull] ──视锥体剔除──▶ VisibleClusters
                                │
                                ▼
[SW Raster CS] ──小三角形软件光栅──▶ VisBuffer64
                                │
                                ▼
                        [DepthExport]
                  SceneDepth + ShadingMask
                                │
                                ▼
                        [ShadeBinning]
                  按材质桶排序像素列表
                                │
                                ▼
                        [ShadeGBuffer CS]
                  解码 VisBuffer → 材质求值
                                │
                                ▼
                        G-Buffer UAV
                  (Albedo + Normal + Roughness)
                                │
                                ▼
                        [Lighting PS]
                  全屏 Deferred Lighting
```

### VisBuffer 格式

```
VisBuffer64 (R64_UINT):
  High 32 bits: [31] = bImposter | [30:7] = VisibleClusterIndex+1 | [6:0] = TriIndex
  Low  32 bits: DepthInt (asuint 编码的 float 深度)

写入方式: ImageInterlockedMaxUInt64 (硬件 64 位原子 Max)
```

如果硬件不支持 64 位图像原子，使用两阶段 32 位原子替代方案（见第五节）。

### Descriptor Set 绑定模型

```
Set 0 (Global, 每帧绑定):
  binding 0: SceneUBO          (ViewProj, CameraPos, LightData)
  binding 1: sampler2DShadow   (Shadow Map, 后续)

Set 1 (Bindless, 初始化时写入):
  binding 0: texture2D[]       (所有纹理的数组, 最多 1024)
  binding 1: MaterialData[]    (SSBO, 材质参数数组)
  binding 2: ClusterPageData   (ByteAddressBuffer, 顶点/索引数据)
```

---

## 二、文件结构

```
src/
├── Core/
│   ├── Platform/
│   │   └── AppWindow.h/.cpp           # GLFW 窗口 + Surface 创建
│   │
│   ├── Math/
│   │   ├── Math.h                     # 聚合头
│   │   ├── MathConfig.h               # 深度约定、精度配置
│   │   ├── MathFwd.h                  # 前向声明
│   │   ├── MathUtility.h              # 标量工具
│   │   ├── Vector.h / Vector4.h       # 向量类型
│   │   ├── Matrix.h                   # 4x4 矩阵
│   │   ├── Plane.h                    # 平面（视锥体）
│   │   └── Frustum.h                 # 视锥体提取 (Gribb/Hartmann)
│   │
│   ├── Graphics/
│   │   ├── VulkanContext.h/.cpp        # Instance + Device + VMA + 队列族
│   │   ├── VulkanSwapchain.h/.cpp      # 交换链 + 深度缓冲
│   │   ├── RenderDevice.h/.cpp         # Buffer/Image/Sampler 创建/销毁 (VMA)
│   │   ├── CommandManager.h/.cpp       # CommandPool + 即时提交 + 布局转换
│   │   ├── ResourceManager.h/.cpp      # 统一 VB/IB + Mesh 索引 + 纹理索引
│   │   ├── DescriptorManager.h/.cpp    # Set 0 + Set 1 (Bindless) 创建和管理
│   │   │
│   │   ├── VisBuffer.h/.cpp           # R64_UINT 纹理创建 + Clear + Readback 调试
│   │   ├── Cluster.h/.cpp             # FPackedCluster 结构 + CPU 端 Cluster 构建
│   │   ├── ClusterUpload.h/.cpp       # Cluster 数据上传到 GPU Buffer
│   │   ├── CullingPass.h/.cpp         # ClusterCull Compute 调度
│   │   ├── RasterPass.h/.cpp          # SW 光栅 (Compute) 调度
│   │   ├── DepthExport.h/.cpp         # VisBuffer → SceneDepth + ShadingMask
│   │   ├── ShadeBinning.h/.cpp        # 按材质桶排序像素
│   │   ├── ShadePass.h/.cpp           # Deferred Material 着色 (Compute)
│   │   ├── LightingPass.h/.cpp        # 全屏 Deferred Lighting
│   │   └── Renderer.h/.cpp            # 帧循环编排 + 初始化/清理
│   │
│   ├── Scene/
│   │   └── Camera.h/.cpp              # FPS 漫游相机 + 视锥体提取
│   │
│   ├── Editor/
│   │   ├── UIManager.h/.cpp            # ImGui 初始化 + 渲染
│   │   └── DebugUI.h/.cpp              # 调试面板 (VisBuffer 预览、可见 Cluster 数)
│   │
│   └── Utils/
│       ├── Log.h/.cpp                  # spdlog 封装
│       └── VulkanUtils.h               # VK_CHECK, GetVector, 格式化
│
└── Examples/
    └── Tumbler/
        ├── main.cpp                    # 入口
        └── AppLogic.h/.cpp             # 场景搭建 + 帧逻辑
```

### 着色器（`assets/shaders/engine/`）

```
cluster_cull.comp       # Cluster 视锥体剔除
rasterize.comp          # SW 光栅化 (Compute) — 边函数 + 原子 VisBuffer 写入
depth_export.comp       # VisBuffer → SceneDepth + ShadingMask
shade_binning.comp      # 像素按材质桶排序
shade_gbuffer.comp      # 解码 VisBuffer → 材质求值 → 写 G-Buffer UAV
lighting.vert           # 全屏三角形
lighting.frag           # Deferred Lighting + PBR + 方向光
```

后续阶段增加：
```
instance_cull.comp      # Instance 视锥体剔除（Phase 6+）
node_cull.comp          # BVH 层级遍历 + LOD 选择（Phase 6+）
raster_bin_build.comp   # 按材质 + 大小分箱（Phase 8+）
rasterize.vert          # HW 光栅化顶点着色器（Phase 8+）
rasterize.frag          # HW 光栅化像素着色器（Phase 8+）
```

---

## 三、GPU 数据布局

### FPackedCluster（128 字节，对标 UE `FPackedCluster`）

```
struct FPackedCluster {
    // 光栅化引用
    uint NumVerts_PositionOffset;     // [13:0]=NumVerts, [31:14]=PositionByteOffset
    uint NumTris_IndexOffset;         // [7:0]=NumTris, [31:8]=IndexByteOffset
    uint AttributeOffset_Bits;        // 法线/切线/UV 的偏移与位宽
    uint ColorOffset_Bits;            // 顶点颜色偏移与位宽
    uint VertReuseBatchInfo[4];       // 顶点复用批次

    // 剔除
    float LODBounds[4];               // xyz = 包围球中心, w = 半径
    float BoxBoundsCenter[3];         // AABB 中心
    float BoxBoundsExtent[3];         // AABB 半尺寸
    float LODError;                   // 几何误差（世界单位），LOD 0 时设为 0
    float EdgeLength;                 // 最长边（像素尺度参考）

    // 材质
    uint MaterialIndex;               // 索引到 MaterialData SSBO（Phase 4: 始终为 0）
    uint AttributeBitWidths;          // [3:0]=NormalBits, [7:4]=TangentBits, ...
    uint DecodeInfoOffset;            // 变长编码数据的偏移
    uint Flags_Padding;               // bTwoSided, bVisible, ...
};
```

**Phase 4 简化**：MaterialIndex 固定为 0（单材质），LODError 固定为 0，顶点不经量化直接存储（float32）。

### ClusterPageData（ByteAddressBuffer）

```
每页布局（顺序排列）:
  [PageHeader]          — 修复信息、Cluster 数量
  [FPackedCluster[N]]   — Cluster 头数组（128 字节对齐）
  [DecodeInfo]          — 变长：三角形索引条带数据
  [PositionData]        — 顶点位置（Phase 4: float32，后续量化）
  [AttributeData]       — 法线、UV 等属性数据
```

---

## 四、分模块开发顺序

### 阶段 0: 分支与清理 ✅
### 阶段 1: 平台层 ✅
### 阶段 2: Vulkan 基础设施 ✅
### 阶段 3: 资源管理 ✅

---

### 阶段 3.1: 资产管线 (Part 2-4) ✅

**目标**: 建立完整的资产导入→运行时加载链

| Part | 模块 | 关键内容 |
|------|------|----------|
| 2 | `TumblerImporter` | OBJ→.tmesh, PNG→.ttex (CPU mipmap), SceneSerializer, asset_map.json |
| 3 | `AssetDatabase` | asset_map.json 加载, 源路径→cooked 路径映射, 按类型查询元数据 |
| 4 | `SceneLoader` + Components | Scene JSON→FScene, CStaticMesh/CCamera/CPointLight/CDirectionalLight |

**文件格式**:
- `.tmesh`: Header(64B) + SubMeshArray(N×44B) + VertexData(float32×8) + IndexData(uint32)
- `.ttex`: Header(32B) + MipData (CPU stbir_resize)
- `.tmat`: JSON, PBR Metallic-Roughness 合并通道
- `.tscene`: JSON, 引用源文件路径 + materials 数组（UE 风格 per-slot 覆盖）

### 阶段 3.2: Engine (Part 5) ✅

**目标**: 生命周期编排 + 依赖注入 + 主循环

| 模块 | 关键内容 |
|------|----------|
| `EngineConfig` | engine.json 读取 (window + render 配置) |
| `Engine` | 按序 Init/Shutdown 8 个子系统, Run() 主循环 (Acquire+Present) |

**依赖注入顺序**: AssetDatabase → AppWindow → VulkanContext → RenderDevice → CommandManager → VulkanSwapchain → DescriptorManager → ResourceManager

### Part 6: 示例应用 ✅

`App-Tumbler` 可执行文件，Engine API 集成，Scene 加载验证。

---

## 阶段 4: LOD 0 Cluster + GPU-Driven Culling + VisBuffer + Deferred

**核心思路**: 跳过 LOD 层级和复杂优化，先把一个完整 Mesh 拆成 Cluster → GPU 剔除 → SW 光栅 → VisBuffer → Deferred Shading 整条链路跑通。拿到第一个可工作的渲染器后，再逐步加 LOD、HZB、混合光栅等优化。

**不做**:
- HZB 遮挡剔除（Phase 8）
- LOD 层级 / BVH DAG（Phase 6）
- HW 硬件光栅（Phase 7）
- 顶点压缩编码（Phase 9）
- 流式加载（Phase 9）

### Phase 4A: VisBuffer 创建 + 单三角形 SW 光栅验证

**目标**: Compute Shader 光栅化单个硬编码三角形 → VisBuffer64，验证最小可工作单元。

| 新文件 | 职责 |
|--------|------|
| `src/Core/Graphics/VisBuffer.h/.cpp` | 创建 `R64_UINT` 格式纹理, `ImageInterlockedMaxUInt64` 原子写入, Clear(0) + Readback 调试 |
| `src/Core/Graphics/RasterPass.h/.cpp` | `rasterize.comp` Compute Pipeline 管理, Dispatch(1,1,1) |
| `assets/shaders/engine/rasterize.comp` | 边函数 + 重心坐标插值深度 + 原子写 VisBuffer64 |

**Shader 关键代码路径**:
```glsl
// rasterize.comp
const ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
const vec3 v0, v1, v2; // 硬编码三角形顶点（裁剪空间）
// 边函数测试
float w0 = edgeFunction(v1, v2, pixel);
float w1 = edgeFunction(v2, v0, pixel);
float w2 = edgeFunction(v0, v1, pixel);
if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
    // 透视校正插值深度
    float depth = /* ... */;
    uint depthInt = floatBitsToUint(depth);
    uint pixelValue = (1u << 7) | 0u; // VisibleClusterIndex=1, TriIndex=0
    uint64_t packed = (uint64_t(pixelValue) << 32) | uint64_t(depthInt);
    imageAtomicMaxUInt64(OutVisBuffer64, pixel, packed);
}
```

**验证**: RenderDoc 查看 VisBuffer64 纹理内容。必要时 Readback 到 CPU 验证像素值正确。

### Phase 4B: Cluster 数据结构 + CPU 构建

**目标**: 定义 `FPackedCluster`（128 字节），CPU 侧把 Mesh 切分为 LOD 0 Cluster。

| 新文件 | 职责 |
|--------|------|
| `src/Core/Graphics/Cluster.h` | `FPackedCluster`（128 字节）、`FCluster`（CPU 构建中间结构）|
| `src/Core/Graphics/Cluster.cpp` | `BuildClusters(vertices[], indices[])` — Morton Code 排序 + 贪婪合并 ~128 三角形/Cluster |

**Cluster 构建流程**:
```
Mesh (vertices[], indices[])
  │  Morton Code 排序 (空间局部性)
  ▼
按每组 64-128 三角形切分 → FCluster[]
  │  每个 FCluster:
  │    - ComputeBoundingSphere()
  │    - ComputeAABB()
  │    - 提取 MaterialIndex (Phase 4: 固定为 0)
  │    - 提取顶点/索引子集（不量化，保持 float32）
  │    - NumVerts, NumTris, PositionByteOffset, IndexByteOffset
  ▼
打包为 FPackedCluster[128B] → ClusterPageData Buffer
```

**Morton Code 排序**: 用顶点世界坐标计算 30-bit Morton Code（X/Y/Z 各 10 bits），对三角形按 Morton 码排序 → 增强局部性 → 提高剔除效率。

**Single material simplification (Phase 4)**: 所有 Cluster 的 `MaterialIndex = 0`。`FPackedCluster` 结构预留该字段，但构建时不执行材质范围排序。

**验证**: 单元测试验证包围球/AABB 覆盖所有顶点，Morton 排序后相邻三角形空间连续。
**参考 UE**: `C:\UnrealEngine\Engine\Source\Developer\NaniteBuilder\Private\Cluster.h` — `FCluster` 定义

### Phase 4C: Cluster 上传 + GPU 视锥体剔除

**目标**: 将 ClusterPageData 上传到 GPU，Compute Shader 执行视锥体剔除。

| 新文件 | 职责 |
|--------|------|
| `src/Core/Graphics/ClusterUpload.h/.cpp` | 上传 ClusterPageData (ByteAddressBuffer) + VisibleClusters Buffer |
| `src/Core/Graphics/CullingPass.h/.cpp` | CPU 侧调度：Cluster Cull Compute → Barrier |
| `assets/shaders/engine/cluster_cull.comp` | 包围球-视锥体相交测试，输出到 VisibleClusters 列表 |

**Shader 关键代码路径**:
```glsl
// cluster_cull.comp
for (uint i = threadId; i < NumClusters; i += GROUP_SIZE) {
    FPackedCluster cluster = ClusterPageData[i];
    if (FrustumIntersectSphere(ViewFrustum, cluster.LODBounds)) {
        uint idx = atomicAdd(VisibleCount, 1);
        VisibleClusters[idx] = i;
    }
}
```

**包围球-视锥体相交**:
```glsl
bool FrustumIntersectSphere(vec4 frustumPlanes[6], vec4 sphere) {
    for (int i = 0; i < 6; i++) {
        if (dot(frustumPlanes[i].xyz, sphere.xyz) + frustumPlanes[i].w + sphere.w < 0.0)
            return false;
    }
    return true;
}
```

**验证**: 从 CPU Readback `VisibleCount` 和 `VisibleClusters`，验证可见 Cluster 数量合理（视锥体内的 Cluster 被保留，视锥体外的被剔除）。

### Phase 4D: 多 Cluster SW 光栅 → VisBuffer64

**目标**: 遍历所有可见 Cluster 的所有三角形，SW 光栅写入 VisBuffer64。

| 修改文件 | 变更 |
|----------|------|
| `rasterize.comp` | 从硬编码三角形改为：读取 VisibleClusters → 读取 ClusterPageData → 解压顶点 → 变换 → 光栅每个三角形 |
| `RasterPass.cpp` | 改为 Indirect Dispatch（每个可见 Cluster 一组），或大 Grid Dispatch with early-out |

**Shader 关键代码路径**:
```glsl
// rasterize.comp (per-pixel dispatch)
uint visibleClusterIndex = /* from push constant / workgroup */;
FPackedCluster cluster = ClusterPageData[VisibleClusters[visibleClusterIndex]];

for (uint triIdx = 0; triIdx < cluster.NumTris; triIdx++) {
    // 解压 3 个顶点索引
    uint i0 = IndexBuffer[cluster.IndexByteOffset/4 + triIdx*3 + 0];
    uint i1 = IndexBuffer[cluster.IndexByteOffset/4 + triIdx*3 + 1];
    uint i2 = IndexBuffer[cluster.IndexByteOffset/4 + triIdx*3 + 2];
    // 解压顶点位置
    vec3 v0_pos = PositionBuffer[cluster.PositionByteOffset/4 + i0*3 ...];
    vec3 v1_pos = PositionBuffer[cluster.PositionByteOffset/4 + i1*3 ...];
    vec3 v2_pos = PositionBuffer[cluster.PositionByteOffset/4 + i2*3 ...];
    // 变换到裁剪空间
    vec4 clip0 = ViewProj * vec4(v0_pos, 1.0);
    // ... 边函数 + 重心插值深度 + imageAtomicMaxUInt64
}
```

**验证**: RenderDoc 查看 VisBuffer64 纹理 — Sword 模型完整渲染无空洞。对比三角形数量验证覆盖率。

**注意**: 此阶段可能有大量 quad-overdraw（小三角形完全被覆盖但调度层面不高效）。这是预期行为 — HW 光栅（Phase 7）会解决。

### Phase 4E: Depth Export（VisBuffer → SceneDepth + ShadingMask）

**目标**: 从 VisBuffer64 解码深度，写入 SceneDepth，生成 ShadingMask。

| 新文件 | 职责 |
|--------|------|
| `src/Core/Graphics/DepthExport.h/.cpp` | `depth_export.comp` Compute Pipeline 管理 |
| `assets/shaders/engine/depth_export.comp` | 读取 VisBuffer64 → 解码深度 → 写 SceneDepth + ShadingMask |

**Shader 关键代码路径**:
```glsl
// depth_export.comp
uint64_t packed = imageLoad(VisBuffer64, pixel).r;
uint depthInt = uint(packed & 0xFFFFFFFFu);
float depth = uintBitsToFloat(depthInt);

// 写入 SceneDepth（R32_SFLOAT）
imageStore(OutSceneDepth, pixel, vec4(depth, 0, 0, 0));

// 解码可见性信息
uint visibleClusterIndex = uint((packed >> 32) & 0xFFFFFFu) >> 7;
uint triIndex = uint((packed >> 32) & 0x7Fu);
bool bVisible = (visibleClusterIndex > 0) || (triIndex > 0);

// 写入 ShadingMask
imageStore(OutShadingMask, pixel, vec4(bVisible ? 1.0 : 0.0, 0, 0, 0));
```

**验证**: RenderDoc 查看 SceneDepth — 应有正确的深度值，与三角形渲染结果一致。

### Phase 4F: Shade Binning + VisBuffer 解码 → G-Buffer

**目标**: 从 VisBuffer 解码三角形信息 → 重心坐标插值 UV/Normal → 材质求值 → 写 G-Buffer UAV。

| 新文件 | 职责 |
|--------|------|
| `src/Core/Graphics/ShadeBinning.h/.cpp` | `shade_binning.comp` — 读取 ShadingMask, 生成像素列表 |
| `src/Core/Graphics/ShadePass.h/.cpp` | `shade_gbuffer.comp` — VisBuffer 解码 + 材质求值 |
| `assets/shaders/engine/shade_binning.comp` | 按 ShadingMask 收集可见像素 |
| `assets/shaders/engine/shade_gbuffer.comp` | 解码 VisBuffer64 → 计算重心坐标 → 插值 UV/Normal → PBR 采样 → 写 G-Buffer UAV |

**流程（单材质简化）**:
```
VisBuffer64[Pixel]
  → 解码: VisibleClusterIndex, TriIndex
  → 读取 ClusterPageData[cluster]
  → 解码 3 个顶点索引 + 3 个变换后顶点位置
  → 计算重心坐标（屏幕空间）
  → 插值: UV, Normal（从 AttributeData 解压）
  → PBR 采样: albedo = texture(MaterialData[0].albedoTexIndex, UV)
  → 输出: OutAlbedo, OutNormal, OutRoughnessMetallic
```

**Phase 4 简化**: 所有像素使用同一种材质（MaterialIndex = 0），无需真正的 Shade Binning 排序。但预留 binning 分发框架。

**验证**: G-Buffer 三个 RT 包含正确的 Albedo、Normal、Roughness/Metallic。RenderDoc 可视化每个通道。

### Phase 4G: Deferred Lighting

**目标**: 从 G-Buffer 读取数据，执行 PBR Deferred Lighting + 方向光。

| 新文件 | 职责 |
|--------|------|
| `src/Core/Graphics/LightingPass.h/.cpp` | 全屏三角形 + Deferred Lighting Pipeline |
| `assets/shaders/engine/lighting.vert` | 全屏三角形顶点着色器 |
| `assets/shaders/engine/lighting.frag` | PBR Cook-Torrance + 方向光 + Gamma 校正 |

**Shader 关键代码路径**:
```glsl
// lighting.frag
vec3 albedo = texture(GBufferA, uv).rgb;
vec3 normal = OctahedronDecode(texture(GBufferB, uv).rg);
float roughness = texture(GBufferB, uv).b;
float metallic = texture(GBufferB, uv).a;

// PBR Cook-Torrance
vec3 F0 = mix(vec3(0.04), albedo, metallic);
vec3 Lo = vec3(0.0);
for (int i = 0; i < NumLights; i++) {
    // BRDF 求值: D (GGX) + G (Smith) + F (Fresnel)
    Lo += CookTorrance(viewDir, lightDir, normal, F0, roughness, metallic);
}
vec3 color = Lo + vec3(0.03) * albedo * (1.0 - metallic); // ambient
color = color / (color + vec3(1.0)); // Reinhard tone map
color = pow(color, vec3(1.0/2.2));   // Gamma
```

**验证**: 屏幕显示 PBR 光照的 Sword 模型。旋转相机验证光照方向正确。

---

## 阶段 5: 多材质支持

**目标**: 单 Mesh 多 Material Slot → 逐 Cluster 材质索引 → Shade Binning 真正按材质桶分发。

| 模块 | 关键内容 |
|------|----------|
| Cluster Material Ranges | CPU 构建时三角形按 MaterialIndex 排序，生成 `FMaterialRange[]` |
| Shade Binning | `shade_binning.comp` — 按 MaterialIndex 排序像素，生成 per-material Indirect Dispatch |
| Multi-Material Shade | `shade_gbuffer.comp` — 不同 MaterialIndex 使用不同 MaterialData 条目 |
| Scene Loading | Scene JSON 支持 `materialSlots: [index, count]` 字段，映射到 Cluster |

**参考 UE**:
- `NaniteEncodeMaterial.cpp` — MaterialRange 构建 (fast path ≤3, slow path ≤64)
- `NaniteRasterBinning.usf` — 逐 MaterialRange 分发 RasterBin
- 详见 `docs/ue-research/nanite-material-system.md`

**验证**: Sword 模型如果只有单材质，则用多材质测试模型（或手动给不同 Cluster 分配不同 MaterialIndex）。同一帧内混合 2+ 种材质的 Cluster 正确渲染。

---

## 阶段 6: Cluster 层级 + LOD 选择

**目标**: 构建 3-5 层 Cluster DAG，GPU 遍历并选择 LOD。

| 模块 | 关键内容 |
|------|----------|
| `ClusterDAG` | CPU 侧构建：递归合并相邻 Cluster → 简化 (edge collapse) → 生成父级 Cluster |
| `HierarchyNode` | `FPackedHierarchyNode` 封装，扇出 8 子节点 |
| `NodeCullCS` | 广度优先遍历：视锥体剔除 → 屏幕误差判定 → 接受或继续下降 |
| `InstanceCullCS` | 多实例剔除（多物体场景） |

**LOD 选择公式**（对标 UE）:
```
projectedError = LODError / (tanHalfFOV * distanceToCamera)
pixelError = projectedError * screenHeight * 0.5
// pixelError < threshold → 接受此 LOD
// pixelError > threshold → 下降到子节点
```

### FPackedHierarchyNode（对标 UE）

```
#define NANITE_MAX_BVH_NODE_FANOUT 8

struct FPackedHierarchyNode {
    // 每个子节点:
    //   LODBounds[4]            — 包围球 (xyz + radius)
    //   BoxBoundsCenter[3]      — AABB 中心
    //   BoxBoundsExtent[3]      — AABB 半尺寸
    //   ChildStartReference     — GPU 页索引 + Cluster 索引
    //   MinLODError             — 子树的最小几何误差
    //   MaxParentLODError       — 父级的最大几何误差
    uint Data[NANITE_BVH_NODE_SLICE_SIZE_DWORDS];  // ~64 DWORDs
};
```

**验证**: 近处高细节 Cluster、远处低细节 Cluster。DebugUI 显示可见 Cluster 数量随距离动态变化。

---

## 阶段 7: HW 硬件光栅化 + 混合光栅

**目标**: 大三角形走硬件光栅（避免 quad overdraw 和 SW 开销）。

| 模块 | 关键内容 |
|------|----------|
| `RasterBinBuildCS` | 按屏幕面积分 Cluster → HW bin (Indirect Draw) / SW bin (Compute Dispatch) |
| `HWRasterVS` | 从 `ClusterPageData` 解压顶点，Vertex Shader 直接读取 |
| `HWRasterPS` | 写 VisBuffer64（像素着色器版本，使用 `interlockedMax` 模拟 64 位原子） |
| `RasterPass` | 混合调度：先 SW Dispatch → 后 HW `vkCmdDrawIndexedIndirect` |

**分箱阈值**（对标 UE）:
```
if (clusterScreenArea < 64 pixels² || triangleCount > 32)
    → SW bin (Compute Raster)
else
    → HW bin (Graphics Indirect)
```

**验证**: 大三角形性能提升（vs 纯 SW），GPU Profiler 验证 HW/SW 管线并行度。

---

## 阶段 8: HZB 遮挡剔除

**目标**: 用上一帧 SceneDepth 构建 HZB，在 ClusterCull 中加入遮挡测试。

| 模块 | 关键内容 |
|------|----------|
| HZB Build | 上一帧 SceneDepth → 逐级 Mip 取 Max（= 最近深度） |
| ClusterCull 改造 | 包围盒屏幕投影 → 采样 HZB → 深度比较 → 被遮挡则跳过 |
| Conservative Raster | HZB 采样使用保守光栅化确保不漏剔除 |

**Set 0 增加**:
```
binding 2: HZBTexture (上一帧 HZB，用于遮挡剔除)
```

---

## 阶段 9: 压缩编码 + 流式加载

| 模块 | 关键内容 |
|------|----------|
| `ClusterEncode` | 顶点位置量化 (相对 Cluster 包围盒)、八面体编码法线、三角形索引条带化 |
| `Streaming` | Streaming Request Buffer (GPU→CPU Readback) → 优先级排序 → 磁盘读取 → Upload + Fixup |

**编码格式**:
```
Position:  quantized = (pos - clusterCenter) / clusterExtent → uintN (每分量 6-14 bits)
Normal:    OctahedronEncode(normal) → uint2 (每分量 5-12 bits)
UV:        quantized = uv * UVScale → uint2 (每分量 8-16 bits)
Index:     Triangle strip + restart → 条带编码（比原始索引小 40-60%）
```

**验证**: 大场景 (>1M 三角形) 在有限 VRAM 下运行时 Cluster 按需加载，无卡顿。

---

## 五、关键技术细节

### Vulkan 必须开启的特性

```cpp
VkPhysicalDeviceVulkan12Features features12{
    .bufferDeviceAddress = VK_TRUE,
    .descriptorIndexing = VK_TRUE,
    .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
    .descriptorBindingPartiallyBound = VK_TRUE,
    .runtimeDescriptorArray = VK_TRUE,
    .drawIndirectCount = VK_TRUE,
};
```

Phase 4 额外需要:
```cpp
VkPhysicalDeviceShaderAtomicInt64Features atomicInt64Features{
    .shaderBufferInt64Atomics = VK_TRUE,
    .shaderSharedInt64Atomics = VK_TRUE,
};
```

### ImageInterlockedMaxUInt64 的替代方案

如果硬件不支持 64 位图像原子操作（某些 AMD GPU），使用两阶段 32 位原子：

```glsl
// 阶段 1: 深度预通道 (32-bit Depth Buffer)
uint depthInt = floatBitsToUint(depth);
uint prevDepth = imageAtomicMax(OutDepthBuffer, pixel, depthInt);
if (depthInt <= prevDepth) return; // 被遮挡

// 阶段 2: 写入 VisBuffer32 (32-bit, 仅存 TriangleID)
imageStore(OutVisBuffer, pixel, uvec4(TriangleID, 0, 0, 0));
```

### 原子操作的内存序

```cpp
// SW Raster → VisBuffer64
// VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT
// Pipeline stage: VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT

// VisBuffer64 → DepthExport
// VK_ACCESS_SHADER_READ_BIT
// Pipeline stage: VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT

// VisBuffer64 → ShadeGBuffer
// VK_ACCESS_SHADER_READ_BIT
// Pipeline stage: VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
```

### 屏幕空间误差计算公式

```
float LODErrorToScreenPixel(float lodError, float distanceToCamera,
                             float tanHalfFOV, float screenHeight) {
    float projectedSize = lodError / (tanHalfFOV * distanceToCamera);
    return projectedSize * screenHeight * 0.5;
}

// DAG 遍历判定:
if (pixelError < LOD_PIXEL_THRESHOLD) {
    AcceptCluster(node);
} else {
    PushChildren(node);
}
```

---

## 六、G-Buffer 格式

| Attachment | Format | 通道分配 |
|------------|--------|----------|
| 0 (Albedo) | `R8G8B8A8_UNORM` | RGB = Albedo, A = AO |
| 1 (Normal+Roughness) | `R16G16B16A16_SFLOAT` | RG = Octahedron Encoded Normal, B = Roughness, A = Metallic |

Lighting Pass 从 G-Buffer 读取（`VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`）。

---

## 七、不做的内容

- **Forward 渲染路径**: 纯 Deferred
- **Material 模板-实例模式**: 简化为 MaterialData SSBO 索引
- **运行时控制台**: 后续再加
- **SSAO**: Phase 2 后续
- **多线程命令录制**: 保持单线程
- **资源异步加载**: 先用同步加载
- **Nanite 的 Tessellation / Displacement**: Phase 2 后续
- **Ray Tracing**: Phase 3 后续
- **Virtual Shadow Maps**: 先用单 Shadow Map 验证

---

## 八、依赖库（复用 vcpkg.json）

- `vulkan` + `vulkan-loader` + `vulkan-memory-allocator`
- `glfw3`
- `glm`
- `imgui` (glfw-binding + vulkan-binding)
- `spdlog`
- `stb`
- `tinyobjloader`
- `gtest` (测试用)
