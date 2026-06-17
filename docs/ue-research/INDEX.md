# UE5 Nanite 源码调研索引

UE5 源码路径：`C:\UnrealEngine\Engine\Source\`

## 调研笔记

| 文档 | 内容 | 状态 |
|------|------|------|
| [Nanite 材质系统](nanite-material-system.md) | 多材质架构、FPackedCluster 编码、RasterBin/ShadeBin 分发 | ✅ 完成 |

## 待调研

| 主题 | UE 源文件 | 优先级 |
|------|-----------|--------|
| Cluster 构建算法 | `Developer/NaniteBuilder/Private/Cluster.cpp` | Phase 4B |
| Cluster 切分 + Morton Sort | `Developer/NaniteBuilder/Private/ClusterDAG.cpp` | Phase 4B |
| GPU Culling (NodeCull + ClusterCull) | `Renderer/Private/Nanite/NaniteCullRaster.cpp` | Phase 4C |
| SW Raster 实现 | `Shaders/Private/Nanite/NaniteRasterizer.usf` | Phase 4D |
| VisBuffer 解码 + G-Buffer 导出 | `Shaders/Private/Nanite/NaniteExportGBuffer.usf` | Phase 4F |
| HZB 构建 + 遮挡剔除 | `Renderer/Private/Nanite/NaniteHZBCull.cpp` | Phase 8 |
| LOD 层级 + DAG 构建 | `Developer/NaniteBuilder/Private/ImposterBuilder.cpp` | Phase 6 |
| 顶点压缩编码 | `Developer/NaniteBuilder/Private/Encode/NaniteEncode.cpp` | Phase 9 |
| 流式加载 | `Renderer/Private/Nanite/NaniteStreaming.cpp` | Phase 9 |

## 关键源文件速查

### Runtime (Renderer)

```
Engine/Source/Runtime/Renderer/Private/Nanite/
├── NaniteCullRaster.cpp          # GPU Culling 调度 + RasterBin 构建
├── NaniteMaterials.cpp           # 材质槽注册 + Bin 分配
├── NaniteMaterialsSceneExtension.cpp  # GPU Buffer 管理 (MaterialSlot 数据上传)
├── NaniteDrawList.cpp            # 材质管线注册 (RasterBin + ShadingBin)
├── NaniteShading.cpp             # Deferred Shading 调度
├── NaniteVisibility.cpp          # Visibility Buffer 解析
├── NaniteHZBCull.cpp             # HZB 遮挡剔除
└── NaniteStreaming.cpp           # 流式加载管理
```

### Shaders

```
Engine/Shaders/Private/Nanite/
├── NaniteRasterBinning.usf       # RasterBin 构建 (材质分发)
├── NaniteRasterizer.usf          # SW 光栅化
├── NaniteShadeBinning.usf        # 按材质桶排序像素
├── NaniteExportGBuffer.usf       # VisBuffer 解码 → G-Buffer
├── NaniteAttributeDecode.ush     # 顶点/材质解码公共函数
├── NaniteCulling.ush             # 剔除公共函数 (视锥体/HZB)
├── NaniteDataDecode.ush          # Cluster 数据解码
└── NaniteVisualize.usf           # 调试可视化
```

### Builder (Developer)

```
Engine/Source/Developer/NaniteBuilder/Private/
├── Cluster.h                     # FCluster 定义 (CPU 构建中间结构)
├── Cluster.cpp                   # Cluster 构建 + Material Ranges
├── ClusterDAG.cpp                # DAG 层级构建
├── Encode/
│   ├── NaniteEncode.cpp          # 顶点压缩编码
│   └── NaniteEncodeMaterial.cpp  # 材质索引编码 (Fast/Slow path)
└── ImposterBuilder.cpp           # Imposter 生成
```

### Public Headers

```
Engine/Source/Runtime/
├── Engine/Public/
│   ├── NaniteSceneProxy.h        # FMaterialSection, Nanite 场景代理
│   └── Rendering/NaniteResources.h  # FPackedCluster 定义
└── Renderer/Public/
    ├── PrimitiveSceneInfo.h      # FNaniteRasterBin, FNaniteShadingBin, FNaniteMaterialSlot
    └── NaniteMaterials.h         # FNaniteMaterialSlot 结构
```
