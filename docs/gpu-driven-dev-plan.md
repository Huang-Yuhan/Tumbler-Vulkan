# Nanite-Like GPU-Driven 渲染器开发计划

## 概述

在 `gpu-driven-rewrite` 分支上从零重写。目标：复现 UE5 Nanite 的核心管线——**Visibility Buffer + Cluster 层级 + SW/HW 混合光栅 + Deferred Material**。

参考引擎：`C:\UnrealEngine\Engine\Source\Runtime\Renderer\Private\Nanite\`

---

## 一、架构总览

### 每帧 GPU 管线

```
Compute  │ 1. InstanceCull       → 剔除不可见实例
Compute  │ 2. NodeCull            → BVH 层级遍历 + 屏幕误差 LOD 选择
Compute  │ 3. ClusterCull         → 逐 Cluster 视锥体 + HZB 遮挡
Compute  │ 4. RasterBinBuild      → 按材质分箱（HW bin / SW bin）
Compute  │ 5. SW Raster            → 小三角形 Compute 光栅 → 原子写 VisBuffer64
Graphics │ 6. HW Raster (Indirect) → 大三角形硬件光栅 → 原子写 VisBuffer64
Compute  │ 7. DepthExport          → VisBuffer64 解码深度 → SceneDepth + ShadingMask
Compute  │ 8. ShadeBinning         → 按材质桶排序像素
Compute  │ 9. ShadeGBuffer         → 解码 VisBuffer → 材质求值 → 写 G-Buffer UAV
Graphics │ 10. Lighting             → 标准 Deferred Lighting（全屏三角形）
Graphics │ 11. ImGui                → 编辑器叠加
```

### 数据流

```
Mesh 资产（OBJ/GLTF）
  │  CPU 预处理
  ▼
Cluster DAG（层级 LOD 树）
  │  Upload
  ▼
GPU Buffer: ClusterPageData + HierarchyBuffer
  │
  ▼
[NodeCull] ──遍历层级──▶ CandidateClusters
                            │
                            ▼
[ClusterCull] ──视锥体+HZB──▶ VisibleClusters
                                │
                                ▼
[RasterBin] ──按材质+大小分箱──▶ RasterBinMeta
                                │
                    ┌───────────┴───────────┐
                    ▼                       ▼
            [SW Raster CS]          [HW Raster VS/PS]
            小三角形光栅化           大三角形硬件光栅
                    │                       │
                    └───────────┬───────────┘
                                ▼
                        VisBuffer64
                  (TriangleID + Depth)
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

### Descriptor Set 绑定模型

```
Set 0 (Global, 每帧绑定):
  binding 0: SceneUBO          (ViewProj, CameraPos, LightData, HZB 参数)
  binding 1: sampler2DShadow   (Shadow Map)
  binding 2: HZBTexture        (上一帧 HZB，用于遮挡剔除)

Set 1 (Bindless, 初始化时写入):
  binding 0: texture2D[]       (所有纹理的数组, 最多 1024)
  binding 1: MaterialData[]    (SSBO, 材质参数数组)
  binding 2: ClusterPageData   (ByteAddressBuffer, 压缩编码的顶点/索引)
  binding 3: HierarchyBuffer   (ByteAddressBuffer, BVH 节点切片)
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
│   │   ├── VisBuffer.h/.cpp           # R64_UINT 纹理创建 + Clear + 解码
│   │   ├── Cluster.h/.cpp             # FPackedCluster 结构 + CPU 端 Cluster 构建
│   │   ├── ClusterUpload.h/.cpp       # Cluster + Hierarchy 数据上传到 GPU Buffer
│   │   ├── RasterPass.h/.cpp          # SW 光栅 (Compute) + HW 光栅 (Graphics Indirect)
│   │   ├── CullingPass.h/.cpp         # NodeCull + ClusterCull Compute 调度
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
instance_cull.comp      # Instance 视锥体剔除
node_cull.comp          # BVH 层级遍历 + LOD 选择
cluster_cull.comp       # Cluster 视锥体 + HZB 遮挡剔除
raster_bin_build.comp   # 按材质 + 大小分箱
rasterize.comp          # SW 光栅化 (Compute) — 边函数 + 原子 VisBuffer 写入
rasterize.vert          # HW 光栅化顶点着色器 — 从 Cluster Buffer 解压顶点
rasterize.frag          # HW 光栅化像素着色器 — 写 VisBuffer64
depth_export.comp       # VisBuffer → SceneDepth + ShadingMask
shade_binning.comp      # 像素按材质桶排序
shade_gbuffer.comp      # 解码 VisBuffer → 材质求值 → 写 G-Buffer UAV
lighting.vert           # 全屏三角形
lighting.frag           # Deferred Lighting + PBR + 阴影
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
    float LODError;                   // 几何误差（世界单位）
    float EdgeLength;                 // 最长边（像素尺度参考）

    // 材质
    uint MaterialIndex;               // 索引到 MaterialData SSBO
    uint AttributeBitWidths;          // [3:0]=NormalBits, [7:4]=TangentBits, ...
    uint DecodeInfoOffset;            // 变长编码数据的偏移
    uint Flags_Padding;               // bTwoSided, bVisible, ...
};
```

### FPackedHierarchyNode（对标 UE `FPackedHierarchyNode`）

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
    //
    // 实际存储为 4 个 DWORD 切片，每个切片包含 2 个子节点
    uint Data[NANITE_BVH_NODE_SLICE_SIZE_DWORDS];  // ~64 DWORDs
};
```

### ClusterPageData（ByteAddressBuffer）

```
每页布局（顺序排列）:
  [PageHeader]          — 修复信息、Cluster 数量
  [FPackedCluster[N]]   — Cluster 头数组（128 字节对齐）
  [DecodeInfo]          — 变长：三角形索引条带数据
  [PositionData]        — 变长：量化顶点位置
  [AttributeData]       — 变长：法线(八面体编码)、切线、UV、颜色
```

---

## 四、分模块开发顺序

### 阶段 0: 分支与清理 ✅
### 阶段 1: 平台层 ✅
### 阶段 2: Vulkan 基础设施 ✅
### 阶段 3: 资源管理 ✅

---

### 阶段 4: Visibility Buffer + 单三角形 SW 光栅

**目标**: Compute Shader 光栅化单个硬编码三角形 → VisBuffer64

| 模块 | 关键内容 |
|------|----------|
| `VisBuffer` | 创建 `R64_UINT` 格式纹理, `ImageInterlockedMaxUInt64` 原子写入, Clear(0) + Readback 调试 |
| `RasterPass` | `rasterize.comp` — 边函数计算、重心坐标插值深度、原子写 VisBuffer64, Dispatch(1,1,1) |
| `DepthExport` | `depth_export.comp` — 读取 VisBuffer64, 解码深度写入 SceneDepth |

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
    imageAtomicMaxUint64(OutVisBuffer64, pixel, packed);
}
```

**验证**: 一个彩色三角形出现在屏幕中央，RenderDoc 可查看 VisBuffer64 纹理内容。

---

### 阶段 5: Cluster 数据结构 + CPU 预处理

**目标**: 定义 Cluster 格式，CPU 侧把网格切分为 Cluster 并上传

| 模块 | 关键内容 |
|------|----------|
| `Cluster.h` | `FPackedCluster`（128 字节）、`FCluster`（CPU 构建中间结构） |
| `ClusterBuilder` | `BuildClusters(vertices[], indices[])` — 按空间 Morton Code 排序 + 贪婪合并到 ~128 三角形/Cluster |
| `ClusterUpload` | 上传 `ClusterPageData` (ByteAddressBuffer) 到 GPU |

**Cluster 构建流程**:
```
Mesh (vertices[], indices[])
  │  Morton Code 排序 (空间局部性)
  ▼
按每组 64-128 三角形切分 → FCluster[]
  │  每个 FCluster:
  │    - ComputeBoundingSphere()
  │    - ComputeAABB()
  │    - 提取材质 ID
  │    - 量化顶点位置 (相对 Cluster 包围盒中心)
  ▼
打包为 FPackedCluster[] → ClusterPageData Buffer
```

**验证**: RenderDoc 查看 GPU Buffer 中 Cluster 数据，验证包围球/AABB 与原始网格匹配。

---

### 阶段 6: GPU Cluster 光栅化（无 LOD，单层级）

**目标**: Compute Shader 遍历所有 Cluster，光栅化完整模型

| 模块 | 关键内容 |
|------|----------|
| `ClusterCullCS` | 视锥体剔除（球体-平面测试），输出 `VisibleClusters` |
| `ClusterRasterCS` | 读取 `VisibleClusters` → 解压顶点 → 变换 → SW 光栅所有三角形 |
| `CullingPass` | CPU 侧调度：先 Cull → Barrier → Raster |

**Shader 改造**:
```glsl
// cluster_cull.comp
for (uint i = threadId; i < NumClusters; i += GROUP_SIZE) {
    FPackedCluster cluster = ClusterPageData[i];
    if (FrustumIntersectSphere(ViewFrustum, cluster.LODBounds)) {
        uint idx = atomicAdd(VisibleCount, 1);
        VisibleClusters[idx] = i;
    }
}

// rasterize.comp (per-cluster dispatch)
for (uint triIdx = 0; triIdx < cluster.NumTris; triIdx++) {
    // 解压三角形索引 → 解压顶点 → 变换 → 光栅
}
```

**验证**: Stanford Bunny (~70K 三角形) 正确渲染。RenderDoc 验证 VisBuffer 无空洞。

---

### 阶段 7: Cluster 层级 + LOD 选择

**目标**: 构建 3-5 层 Cluster DAG，GPU 遍历并选择 LOD

| 模块 | 关键内容 |
|------|----------|
| `ClusterDAG` | CPU 侧构建：递归合并相邻 Cluster → 简化 (edge collapse) → 生成父级 Cluster |
| `HierarchyNode` | `FPackedHierarchyNode` 封装，扇出 8 子节点 |
| `NodeCullCS` | 广度优先遍历：视锥体剔除 → 屏幕误差判定 → 接受或继续下降 |

**LOD 选择公式**（对标 UE）:
```
projectedError = LODError / (tanHalfFOV * distanceToCamera)
pixelError = projectedError * screenHeight * 0.5
// pixelError < threshold → 接受此 LOD
// pixelError > threshold → 下降到子节点
```

**验证**: 近处高细节 Cluster、远处低细节 Cluster。`DebugUI` 显示可见 Cluster 数量随距离动态变化。

---

### 阶段 8: HW 光栅化 + 混合光栅

**目标**: 大三角形走硬件光栅（避免 quad overdraw 和 SW 开销）

| 模块 | 关键内容 |
|------|----------|
| `RasterBinBuildCS` | 按屏幕面积分 Cluster → HW bin (Indirect Draw) / SW bin (Compute Dispatch) |
| `HWRasterVS` | 从 `ClusterPageData` 解压顶点，从 `VisibleClusters` 读 Transform |
| `HWRasterPS` | 写 VisBuffer64（像素着色器版本，使用 `InterlockedMax` 模拟） |
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

### 阶段 9: Deferred Material

**目标**: VisBuffer 解码 → 重心坐标插值 UV/Normal → PBR 材质 → 写 G-Buffer

| 模块 | 关键内容 |
|------|----------|
| `ShadeBinning` | `shade_binning.comp` — 读取 ShadingMask, 按 MaterialIndex 排序像素, 生成 per-material 间接调度参数 |
| `ShadeGBufferCS` | `shade_gbuffer.comp` — 读取 VisBuffer64 → 解码 Cluster → 计算重心坐标 → 插值 UV/Normal/Tangent → 纹理采样 → 写 G-Buffer UAV |
| `MaterialDataSSBO` | 每材质: `{ albedoTexIndex, normalTexIndex, metallicRoughnessTexIndex, roughness, metallic, baseColorTint }` |

**材质着色流程**:
```
VisBuffer64[Pixel]
  → 解码: VisibleClusterIndex, TriIndex
  → 读取 ClusterPageData[cluster.PageIndex]
  → 解码 3 个顶点索引 (+ 3 个变换后顶点位置)
  → 计算重心坐标 (屏幕空间 → 世界空间)
  → 插值: UV, Normal, Tangent
  → PBR 采样: albedo = texture(bindless[mat.albedoTexIndex], UV)
  → 输出: OutAlbedo, OutNormal, OutRoughnessMetallic
```

**验证**: 多材质物体正确渲染，同一帧内混合 3+ 种材质。

---

### 阶段 10: 压缩编码 + 流式加载

| 模块 | 关键内容 |
|------|----------|
| `ClusterEncode` | 顶点位置量化 (相对 Cluster 包围盒)、八面体编码法线、三角形索引条带化 (triangle strip) |
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

### VulkanContext 必须开启的特性

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

额外需要的特性（阶段 8+）:
- `VK_EXT_mesh_shader`（可选，对标 Nanite 的 Mesh Shader 路径）
- `shaderInt64`（VisBuffer64 需要 64 位原子操作）

检查 `VkPhysicalDeviceShaderAtomicInt64Features`:
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

### Cluster 包围球-视锥体相交测试

```glsl
bool FrustumIntersectSphere(mat4x3 frustumPlanes, vec4 sphere) {
    // 6 个平面测试: dot(plane, sphere.xyz) + sphere.w < 0 → 剔除
    for (int i = 0; i < 6; i++) {
        if (dot(frustumPlanes[i], vec4(sphere.xyz, 1.0)) + sphere.w < 0.0)
            return false;
    }
    return true;
}
```

### 屏幕空间误差计算公式

```
// World-space error → Screen-space pixel error
float LODErrorToScreenPixel(float lodError, float distanceToCamera, 
                             float tanHalfFOV, float screenHeight) {
    float projectedSize = lodError / (tanHalfFOV * distanceToCamera);
    return projectedSize * screenHeight * 0.5;
}

// DAG 遍历判定:
if (pixelError < LOD_PIXEL_THRESHOLD) {
    AcceptCluster(node);  // 当前 LOD 足够，停止下降
} else {
    PushChildren(node);   // 需要更高细节，下降到子节点
}
```

### 原子操作的内存序

```cpp
// SW Raster → VisBuffer64
// 使用 VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT
// Pipeline stage: VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT

// HW Raster → VisBuffer64 (需要在 Pixel Shader 中使用原子)
// 使用 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
// Pipeline stage: VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT

// VisBuffer64 → DepthExport
// 使用 VK_ACCESS_SHADER_READ_BIT
// Pipeline stage: VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT

// VisBuffer64 → ShadeGBuffer
// 使用 VK_ACCESS_SHADER_READ_BIT
// Pipeline stage: VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
```

---

## 六、G-Buffer 格式

| Attachment | Format | 通道分配 |
|------------|--------|----------|
| 0 (Albedo) | `R8G8B8A8_UNORM` | RGB = Albedo, A = AO |
| 1 (Normal+Roughness) | `R16G16B16A16_SFLOAT` | RG = Octahedron Encoded Normal, B = Roughness, A = Metallic |

Lighting Pass 从 UAV 写入的 G-Buffer 读取（`VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`）。

---

## 七、不做的内容

- **Forward 渲染路径**: 纯 Deferred
- **ECS 系统**: 无 Actor/Component，数据直接管理
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
