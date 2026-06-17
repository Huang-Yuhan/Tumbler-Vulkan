# UE5 Nanite 材质系统

调研日期：2026-06-17

---

## 核心结论

**Nanite 完全支持单 Mesh 多材质，材质粒度是逐三角形的。** 每个 Cluster 最多 64 种材质（`MaterialIndex` 为 6-bit），其中 ≤3 种走快速内联编码路径。材质在 Raster 和 Shading 阶段分别分发到不同的 Bin。

---

## 一、数据流总览

```
Mesh 资产 (多 Material Slot)
  │  NaniteBuilder
  ▼
FMaterialSection[] (per-slot)
  │  每个 Section: MaterialIndex, RasterMaterialProxy, ShadingMaterialProxy
  ▼
FCluster[] (per-cluster)
  │  三角形按 MaterialIndex 排序 → FMaterialRange[]
  │  Fast Path (≤3): Material0/1/2Index 直接编码进 FPackedCluster
  │  Slow Path (4-64): 存 MaterialTableOffset + Length
  ▼
上传到 GPU:
  ├── ClusterPageData (ByteAddressBuffer) — FPackedCluster + 顶点/索引
  └── MaterialSlot Buffer — FNaniteMaterialSlot[] (RasterBin + ShadingBin indices)
```

---

## 二、关键数据结构

### FMaterialSection（CPU 端，per Mesh）

**文件**: `Engine/Source/Runtime/Engine/Public/NaniteSceneProxy.h:220-267`

```cpp
struct FMaterialSection
{
    FMaterialRenderProxy* RasterMaterialProxy = nullptr;
    FMaterialRenderProxy* ShadingMaterialProxy = nullptr;
    int32 MaterialIndex = INDEX_NONE;      // 映射到 MaterialSlot 数组
    // 以及 per-section 属性（DisplacementScaling, bTwoSided 等）
};
```

每个 Nanite 场景代理持有 `TArray<FMaterialSection> MaterialSections`，数量 = Mesh 的 Material Slot 数量。

### FNaniteMaterialSlot（GPU 端，per Section per MeshPass）

**文件**: `Engine/Source/Runtime/Renderer/Public/NaniteMaterials.h:15-42`

```cpp
struct FNaniteMaterialSlot
{
    uint16 TriangleShadingBin;   // 三角形着色 Bin
    uint16 VoxelShadingBin;      // Voxel 着色 Bin（非关键路径）
    uint16 RasterBin;            // 光栅化 Bin（带 WPO/TwoSided/Displacement）
    uint16 FallbackRasterBin;    // 回退 Bin（深度偏移等特殊材质）
};
```

每个 Primitive 持有 3 个 `TArray<FNaniteMaterialSlot>`（每个 MeshPass 一个），通过 `PrimitiveSceneInfo.h:301-303` 管理。

### FPackedCluster 中的材质编码（GPU 端）

**文件**: `Engine/Source/Runtime/Engine/Public/Rendering/NaniteResources.h:92-120`

```cpp
struct FPackedCluster
{
    // ...
    uint32 PackedMaterialInfo;    // 材质信息（Fast/Slow path 自动选择）
    uint32 VertReuseBatchInfo[4]; // 顶点复用批次，与 MaterialRange 对齐
    // ...
};
```

### PackedMaterialInfo 编码格式

**文件**: `Engine/Source/Developer/NaniteBuilder/Private/Encode/NaniteEncodeMaterial.cpp:30-67`

**Fast Path**（≤3 种材质 per Cluster）:
```
 uint Material0Index  : 6;   // max 64
 uint Material1Index  : 6;
 uint Material2Index  : 6;
 uint Material0Length : 7;   // triangles in range 0 (minus one), max 128
 uint Material1Length : 7;   // triangles in range 1 (minus one), max 64
 // Material2Length = NumTris - Material0Length - Material1Length (implicit)
```

**Slow Path**（4-64 种材质 per Cluster）:
```
 uint BufferIndex     : 19;  // offset into per-primitive material table
 uint BufferLength    : 6;   // number of material ranges (max 64)
 uint Padding         : 7;   // always 127 (discriminator from fast path)
```

**判别逻辑**: `Padding == 127` → Slow Path; 否则 → Fast Path（Padding 在 Fast Path 中 ≤ 1）。

---

## 三、构建流程

### 3.1 三角形按材质排序

**文件**: `Engine/Source/Developer/NaniteBuilder/Private/NaniteEncodeMaterial.cpp:126-177`

```cpp
void BuildMaterialRanges(TArray<FCluster>& Clusters)
{
    ParallelFor(Clusters, [&](uint32 ClusterIndex) {
        FCluster& Cluster = Clusters[ClusterIndex];
        Cluster.BuildMaterialRanges();  // 按 MaterialIndex 排序三角形
    });
}
```

每个 `FCluster` 生成:
```cpp
TArray<int32> MaterialIndexes;            // per-triangle 材质索引
TArray<FMaterialRange> MaterialRanges;    // 排序后的连续范围
```

```cpp
struct FMaterialRange
{
    uint32 RangeStart;    // 起始三角形索引
    uint32 RangeLength;   // 三角形数量
    uint32 MaterialIndex; // 材质索引
    TArray<uint8, TInlineAllocator<12>> BatchTriCounts; // 顶点复用批次
};
```

### 3.2 编码决策

```cpp
if (Cluster.MaterialRanges.Num() <= 3) {
    PackedMaterialInfo = PackMaterialFastPath(/* ... */);
} else {
    PackedMaterialInfo = PackMaterialSlowPath(/* ... */);
}
```

---

## 四、Raster Binning（GPU 端）

**文件**: `Engine/Shaders/Private/Nanite/NaniteRasterBinning.usf:89-761`

### 4.1 材质→RasterBin 查询

```hlsl
uint GetMaterialRasterBinFromIndex(uint RelativeMaterialIndex, uint PrimitiveIndex, ...)
{
    FNaniteMaterialSlot MaterialSlot = LoadMaterialSlot(RelativeMaterialIndex, ...);
    uint RasterBin = MaterialSlot.RasterBin;
    // FallbackRasterBin 用于深度偏移等特殊材质
    if (bFallbackRasterBin && MaterialSlot.FallbackRasterBin != INVALID_BIN)
        RasterBin = MaterialSlot.FallbackRasterBin;
    return RasterBin;
}
```

### 4.2 Fast Path 展开

```hlsl
if (IsMaterialFastPath(Cluster)) {
    RasterBin0 = GetRemappedRasterBinFromIndex(Cluster.Material0Index, ...);
    RasterBin1 = GetRemappedRasterBinFromIndex(Cluster.Material1Index, ...);
    RasterBin2 = GetRemappedRasterBinFromIndex(Cluster.Material2Index, ...);

    // 合并相邻同 Bin 的 Range
    if (RasterBin0 == RasterBin1 && bAllowMerging0 && bAllowMerging1) { /* merge */ }
    // ...
    ExportRasterBin(RasterBin0, /* triangle range 0 */);
    ExportRasterBin(RasterBin1, /* triangle range 1 */);
    ExportRasterBin(RasterBin2, /* triangle range 2 */);
}
```

### 4.3 Slow Path 循环

```hlsl
else {
    for (uint TableEntry = 0; TableEntry < Cluster.MaterialTableLength; ++TableEntry) {
        DecodeMaterialRange(EncodedRange, TriStart, TriLength, MaterialIndex);
        RasterBin = GetRemappedRasterBinFromIndex(MaterialIndex, ...);
        // 合并相邻同 Bin...
        ExportRasterBin(RasterBin, /* triangle range */);
    }
}
```

**关键优化**: 相邻且同 RasterBin 的 MaterialRange 会被合并 → 减少 Dispatch 数量。

---

## 五、Shading（GPU 端）

### 5.1 材质→ShadingBin 查询

**文件**: `Engine/Shaders/Private/Nanite/NaniteAttributeDecode.ush:423-462`

```hlsl
uint GetMaterialShadingBin(FCluster InCluster, uint InPrimitiveIndex,
                           uint InMeshPassIndex, uint InTriIndex)
{
    const uint RelativeMaterialIndex = GetRelativeMaterialIndex(InCluster, InTriIndex);
    return GetMaterialShadingBinFromIndex(RelativeMaterialIndex, InPrimitiveIndex,
                                          InMeshPassIndex, InCluster.bVoxel);
    // 返回 MaterialSlot.TriangleShadingBin 或 MaterialSlot.VoxelShadingBin
}
```

### 5.2 逐像素材质解析

```
VisBuffer64[Pixel]
  → 解码: VisibleClusterIndex, TriIndex
  → GetRelativeMaterialIndex(Cluster, TriIndex):
      Fast Path: 根据 TriIndex 在哪个 MaterialRange 内 → Material0/1/2Index
      Slow Path: 查 Material Table 找到对应 MaterialIndex
  → GetMaterialShadingBin → 选择 Shader
  → 执行对应材质 Shader → 写 G-Buffer
```

---

## 六、对 Tumbler 的启示

### Phase 4（当前阶段）简化方案

Sword 模型是单材质 → 所有 Cluster 的 `MaterialIndex = 0`:

```
FPackedCluster:
  PackedMaterialInfo = PackMaterialFastPath(
      Material0Length = NumTris,  // 全部三角形
      Material0Index  = 0,        // 单一材质
      Material1Index  = 0,        // unused
      Material2Index  = 0         // unused
  );
```

Raster 和 Shading 阶段跳过 Bin 分发，所有像素走同一材质管线。

### Phase 5（多材质）需要实现

1. **`FPackedCluster.MaterialIndex`** — 单材质时就是 `uint`，多材质时改为 `PackedMaterialInfo`（Fast/Slow 编码）
2. **Material Ranges 构建** — CPU 侧在 `BuildClusters()` 中按 MaterialIndex 排序三角形
3. **`MaterialSlot` Buffer** — per-Primitive 的 RasterBin/ShadingBin 查找表
4. **Shade Binning** — `shade_binning.comp` 真正按 MaterialIndex 分类像素，生成 per-material dispatch

### 设计建议

从 Phase 4 开始预留编码位，但实际逻辑从简：

- `FPackedCluster` 用 128 字节对齐，留足 Material 字段空间
- `MaterialIndex` 字段在 Phase 4 固定为 0，Phase 5 改为 PackedMaterialInfo
- `ShadePass` 接口预留 `MaterialIndex` 参数，Phase 4 忽略它
- 不希望 Phase 4 → 5 的数据结构重构
