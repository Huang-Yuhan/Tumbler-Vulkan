# 关键设计决策记录

## 为什么纯 GPU-Driven？

**决策**：所有可视化决策（Frustum Culling、Draw Call 生成）在 GPU Compute Shader 中完成。CPU 只负责上传 Dirty Object 数据和编排 Pass。

**理由**：
- 传统 CPU-Driven 模式每个物体一次 `vkCmdDrawIndexed`，N 个物体 = N 次 CPU→GPU 调用
- GPU-Driven 将 N 次调用合并为 1-3 次 `vkCmdDrawIndexedIndirect`
- Compute Shader 并行处理 Culling，远超 CPU 单线程遍历效率
- 为后续 GPU Occlusion Culling 和 Mesh Shader 铺路

**代价**：调试困难（核心逻辑在 Compute Shader 中），需要 Vulkan 1.2 三大特性支持。

## 为什么纯 Deferred，不要 Forward？

**决策**：只保留 Deferred 渲染路径，删除 Forward 路径和策略模式。

**理由**：
- GPU-Driven 的 Multi-Draw Indirect 与 Deferred 天然亲和——所有几何批处理到一个 Indirect Draw
- Forward 路径的光照与几何绑定，无法利用 Indirect Draw 的批处理优势
- 减少代码分支和维护成本

**代价**：透明物体渲染需单独处理（未来可通过 Forward Bucket 或 OIT 解决）。

## 为什么删除 ECS？

**决策**：删除 `FActor` / `Component`，`Scene` 直接管理 `vector<ObjectData>`。

**理由**：
- GPU-Driven 场景的核心数据是连续 GPU Buffer 中的 Object 数组，不是零散的 Actor 对象
- ECS 的组件查找和动态分发与 GPU 并行模型不匹配
- 更简洁的路径：Scene → Object[] → GPUScene SSBO

**代价**：失去组件化灵活性。如需复杂游戏逻辑，可在外层再封装。

## 为什么 Bindless 纹理数组？

**决策**：所有纹理通过一个 `texture2D[]` 描述符数组访问，材质 ID 索引到数组。

**理由**：
- 消除逐材质 DescriptorSet 绑定
- GPU Culling 和 GBuffer Shader 共享同一个 Set 1
- 减少 DescriptorPool 容量压力

**代价**：需要 `descriptorIndexing` 特性，纹理数组大小有限制。

## 为什么 Unified Vertex/Index Buffer？

**决策**：所有 Mesh 存储在一个大 VB 和一个大 IB 中。

**理由**：
- `vkCmdBindVertexBuffers` 一次调用
- Indirect Draw 通过 `firstIndex` / `vertexOffset` 自动定位
- GPU Culling 可直接索引 Mesh 数据地址

**代价**：需要 Sub-allocation 管理。

## 为什么删除 Material 模板-实例模式？

**决策**：`FMaterial` / `FMaterialInstance` 简化为 `MaterialDataSSBO` + 纹理索引。

**理由**：
- Bindless 下每个材质实例只需一个 MaterialData 结构体
- 不需要每实例独立的 `VkDescriptorSet` 和 `UBOBuffer`
- Pipeline 变体通过 Bucket 分组管理

## 为什么 G-Buffer 用 Sampled Image 而非 Input Attachment？

**决策**：Lighting Pass 通过 `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER` 采样 G-Buffer。

**理由**：
- Subpass Input Attachment 限制在同一个 RenderPass 内，阻碍未来引入后处理
- Sampled Image 更灵活，Pass 之间解耦
- 简化 Barrier 管理

**代价**：Tile-Based GPU 上略微损失带宽优势，但可通过 `VK_ATTACHMENT_STORE_OP_DONT_CARE` 优化。

## 暂缓事项

- SSAO / HBAO — GPU-Driven 跑稳后加入
- Forward 透明 Pass — 先做 Opaque
- Mesh Shader — 需评估硬件覆盖率
- 异步资源加载 — 先同步
- 多线程命令录制 — 先单线程
