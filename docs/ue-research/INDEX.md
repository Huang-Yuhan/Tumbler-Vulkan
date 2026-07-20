# UE5 Nanite 源码调研索引

UE5 源码路径：`C:\UnrealEngine\Engine\Source\`

## 调研笔记

| 文档 | 内容 | 状态 |
|------|------|------|
| [Nanite 材质系统](nanite-material-system.md) | 多材质架构、FPackedCluster 编码、RasterBin/ShadeBin 分发 | ✅ 完成 |
| [Cluster 构建算法](nanite-cluster-building.md) | FCluster CPU 中间结构、128 三角形分组、AABB/球体包围盒、Material Range 排序 | ✅ 完成 |
| [Cluster DAG + LOD 层级](nanite-cluster-dag-lod.md) | DAG 自底向上构建、FPackedHierarchyNode、BVH 四叉扇出、Peristent Thread 遍历、LOD Error 单调嵌套 | ✅ 完成 |
| [GPU 剔除管线](nanite-gpu-culling.md) | InstanceCull → NodeCull → ClusterCull、包围盒/球体视锥测试、HZB 遮挡剔除、两遍遮挡系统 | ✅ 完成 |
| [软件光栅化](nanite-sw-raster.md) | Pineda 边缘函数、矩形/扫描线自适应光栅化、256 子像素精度、64 位 VisBuffer 原子写入、填充约定 | ✅ 完成 |
| [VisBuffer 解码 + G-Buffer 导出](nanite-visbuffer-decode-gbuffer.md) | 64 位 VisBuffer 解包、深度导出 + HTile、ShadeBinning 按材质排序、延迟材质评估、重心坐标重建 | ✅ 完成 |
| [顶点压缩编码](nanite-vertex-encoding.md) | 位置相对量化、八面体法线编码、切线角度编码、自定义 UV 浮点格式、通用三角条带索引编码、页级转码 | ✅ 完成 |
| [流式加载](nanite-streaming.md) | GPU→CPU 请求反馈、优先级系统、根页常驻、LOD 作为流式代理、页池 LRU 淘汰、StreamOut 几何导出 | ✅ 完成 |

## 待调研

| 主题 | UE 源文件 | 优先级 |
|------|-----------|--------|
| (全部完成) | | |

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
├── NaniteStreaming.cpp           # 流式加载管理
└── NaniteStreamOut.cpp           # GPU→CPU 几何导出 (Ray Tracing Fallback)
```

### Shaders

```
Engine/Shaders/Private/Nanite/
├── NaniteRasterBinning.usf       # RasterBin 构建 (材质分发)
├── NaniteRasterizer.usf          # SW 光栅化 (Edge Function + Rect/Scanline)
├── NaniteRasterizer.ush          # 三角设置 + 光栅化策略 (SetupTriangle/RasterizeTri)
├── NaniteRasterizationCommon.ush # 公共光栅化函数 (重心坐标、顶点缓存)
├── NaniteShadeBinning.usf        # 按材质桶排序像素 (BinScalarization)
├── NaniteExportGBuffer.usf       # 深度/模板导出 + HitProxy + 编辑器选择
├── NaniteDepthExport.usf         # 深度导出 + HTile 元数据更新
├── NaniteAttributeDecode.ush     # 顶点/材质解码公共函数 (法线/UV/切线)
├── NaniteDataDecode.ush          # Cluster/Page 数据解码 (FCluster, FHierarchyNodeSlice)
├── NaniteCulling.ush             # 剔除公共函数 (视锥体/HZB/候选节点打包)
├── NaniteHZBCull.ush             # HZB 遮挡剔除 (屏幕矩形/Mip选择/深度测试)
├── NaniteInstanceCulling.usf     # 实例剔除 (Instance → BVH Root Node)
├── NaniteClusterCulling.usf      # 集群剔除 (Node Cull + Cluster Cull)
├── NaniteHierarchyTraversal.ush  # BVH 层级遍历 (Peristent Thread / Per-Level)
├── NaniteHierarchyTraversalCommon.ush # 遍历队列状态 (FQueueState)
├── NaniteStreaming.ush           # 流式请求生成 (RequestPageRange)
├── NaniteStreaming.usf           # 页数据 Memcpy
├── NaniteTranscode.usf           # 页级转码 (Disk Format → GPU Format)
├── NaniteWritePixel.ush          # VisBuffer 64 位原子写入
└── NaniteVisualize.usf           # 调试可视化
```

### Builder (Developer)

```
Engine/Source/Developer/NaniteBuilder/Private/
├── Cluster.h                     # FCluster 定义 (CPU 构建中间结构)
├── Cluster.cpp                   # Cluster 构建 + 简化 + Material Ranges + 包围盒
├── ClusterDAG.cpp                # DAG 层级构建 (ReduceMesh → ReduceGroup)
├── Encode/
│   ├── NaniteEncode.cpp          # 位置量化 (Per-Cluster 变长位宽)
│   ├── NaniteEncodeGeometryData.cpp  # 法线/切线/UV 编码 + 属性打包
│   ├── NaniteEncodeMaterial.cpp  # 材质索引编码 (Fast/Slow path)
│   ├── NaniteEncodeHierarchy.cpp # 层级节点编码
│   └── NaniteEncodePageAssignment.cpp # 页分配 (Root Pages / Streaming Pages)
└── GraphPartitioner.cpp          # Metis 风格图分割器
```

### Public Headers

```
Engine/Source/Runtime/
├── Engine/Public/
│   ├── NaniteSceneProxy.h        # FMaterialSection, Nanite 场景代理
│   ├── Rendering/NaniteResources.h  # FPackedCluster, FPackedHierarchyNode 定义
│   └── Rendering/NaniteStreamingManager.h # FStreamingManager 接口
└── Shaders/Shared/NaniteDefinitions.h # 全部 GPU 常量 (位宽限制/标志位/结构体)
```
