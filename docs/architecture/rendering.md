# 渲染架构

纯 GPU-Driven + Deferred 渲染管线。所有可见性决策在 GPU Compute Shader 中完成，绘制命令通过 Indirect Draw 执行。

## 1. 帧循环

```
Renderer::Render()
  │
  ├─ 1. Sync GPU Scene Data
  │     GPUScene::UploadDirtyObjects()     // CPU dirty → GPU SSBO
  │
  ├─ 2. Compute: Frustum Culling
  │     CullingPass::Execute()
  │       ├─ vkCmdBindPipeline(frustum_cull)
  │       ├─ vkCmdBindDescriptorSets(Set0 + Set1)
  │       └─ vkCmdDispatch(groupCount, 1, 1)
  │     输出: IndirectDraw Buffer + VisibleCount Buffer
  │
  ├─ 3. Barrier (Compute → Graphics + Indirect Read)
  │
  ├─ 4. Shadow Depth Pass
  │     DepthPass::Execute()
  │       ├─ Shadow frustum culling (compute)
  │       ├─ vkCmdBeginRenderPass(shadowRP)
  │       ├─ vkCmdBindVertexBuffers(bigVBBuffer)
  │       ├─ vkCmdBindIndexBuffer(bigIBBuffer)
  │       └─ vkCmdDrawIndexedIndirect(shadowIndirectBuffer, ...)
  │
  ├─ 5. GBuffer Pass (MRT)
  │     GBufferPass::Execute()
  │       ├─ vkCmdBeginRenderPass(gbufferRP)
  │       ├─ vkCmdBindPipeline(gbufferPipeline)
  │       ├─ vkCmdBindDescriptorSets(Set0 + Set1)
  │       ├─ vkCmdBindVertexBuffers(bigVBBuffer)
  │       ├─ vkCmdBindIndexBuffer(bigIBBuffer)
  │       └─ vkCmdDrawIndexedIndirect(mainIndirectBuffer, countBuffer)
  │     输出: Albedo + NormalRoughness + Depth
  │
  ├─ 6. Barrier (GBuffer Write → Lighting Read)
  │
  ├─ 7. Lighting Pass (Fullscreen Triangle)
  │     LightingPass::Execute()
  │       ├─ vkCmdBeginRenderPass(lightingRP)  // 输出到 Swapchain
  │       ├─ vkCmdBindPipeline(lightingPipeline)
  │       ├─ vkCmdBindDescriptorSets(Set0 + GBuffer images)
  │       └─ vkCmdDraw(3, 1, 0, 0)
  │
  └─ 8. ImGui Pass
        UIManager::Render()
```

## 2. GPU Frustum Culling

### Compute Shader 输入

- `ObjectDataSSBO` — 所有物体的 Transform + MeshIndex + MaterialIndex
- `MeshDataSSBO` — 每个 Mesh 的 BoundingSphere
- `SceneUBO` — Frustum Planes（6 个 vec4，从 ViewProj 矩阵提取）

### Compute Shader 输出

- `IndirectDraw Buffer` — `VkDrawIndexedIndirectCommand[]`，仅包含可见物体
- `VisibleCount Buffer` — `atomicAdd` 计数的可见实例数

### 算法

```glsl
// frustum_cull.comp
for (uint i = gl_GlobalInvocationID.x; i < totalObjects; i += gl_WorkGroupSize.x) {
    ObjectGPUData obj = objectSSBO.objects[i];
    MeshGPUData mesh = meshSSBO.meshes[obj.meshIndex];

    // Sphere-Frustum test
    vec4 sphere = mesh.boundingSphere;
    vec3 worldCenter = (obj.modelMatrix * vec4(sphere.xyz, 1.0)).xyz;
    float worldRadius = sphere.w * maxScale(obj.modelMatrix);

    bool visible = true;
    for (int p = 0; p < 6; p++) {
        float dist = dot(vec4(worldCenter, 1.0), frustumPlanes[p]);
        if (dist < -worldRadius) { visible = false; break; }
    }

    if (visible) {
        uint slot = atomicAdd(visibleCount, 1);
        indirectCommands[slot].indexCount = mesh.indexCount;
        indirectCommands[slot].instanceCount = 1;
        indirectCommands[slot].firstIndex = 0;
        indirectCommands[slot].vertexOffset = 0;
        indirectCommands[slot].firstInstance = i;  // objectIndex → vertex shader
    }
}
```

## 3. Indirect Draw

每帧开始前重置 count，绘制时消费 GPU 生成的命令：

```cpp
// CPU 端清零
vkCmdFillBuffer(cmd, countBuffer, 0, sizeof(uint32_t), 0);

// 绘制
vkCmdDrawIndexedIndirect(cmd,
    indirectDrawBuffer,  0,
    drawCount,           // 从 countBuffer 读取
    sizeof(VkDrawIndexedIndirectCommand));
```

## 4. Unified Vertex/Index Buffer

所有 Mesh 共享统一的大 Buffer：

- 初始化时分配 64MB Vertex Buffer + 16MB Index Buffer
- 每个 Mesh 上传时 sub-allocate，记录 offset
- `MeshDataSSBO` 存储每个 Mesh 的 `vertexAddress` 和 `indexAddress`（`VkDeviceAddress`）
- 绘制前一次绑定：`vkCmdBindVertexBuffers(cmd, 0, 1, &bigVBBuffer, &zeroOffset)`
- Indirect Draw 中的 `firstIndex` / `vertexOffset` 自动定位到正确的 Mesh

## 5. G-Buffer 渲染

### Vertex Shader

从 `ObjectDataSSBO` 读取 Transform，使用 `gl_BaseInstance` 索引：

```glsl
layout(set = 1, binding = 2) readonly buffer ObjectData {
    ObjectGPUData objects[];
} objectSSBO;

void main() {
    ObjectGPUData obj = objectSSBO.objects[gl_BaseInstance];
    gl_Position = scene.ViewProj * obj.modelMatrix * vec4(inPosition, 1.0);
    // compute TBN, pass to fragment
}
```

### Fragment Shader

输出到 2 个 MRT Attachment：

```glsl
layout(location = 0) out vec4 outAlbedo;        // R8G8B8A8
layout(location = 1) out vec4 outNormalRoughness; // R16G16B16A16

void main() {
    outAlbedo = vec4(albedo, 1.0);
    outNormalRoughness = vec4(encodeOctahedron(N), roughness, metallic);
}
```

## 6. Lighting Pass

全屏三角形 + PBR：
- 从 G-Buffer 采样 Albedo + Normal + Roughness + Metallic
- 深度反投影重建世界坐标
- PCF 采样 Shadow Map
- Cook-Torrance BRDF + 方向光

## 7. Pipeline Bucket（多材质）

按材质模板分组（Opaque / TwoSided），Culling Compute 按 `materialIndex` 映射的 `bucketIndex` 分组写入不同 Indirect Buffer：

```cpp
for (auto& bucket : pipelineBuckets) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bucket.pipeline);
    vkCmdDrawIndexedIndirect(cmd, bucket.indirectBuffer, 0,
        bucket.drawCount, sizeof(VkDrawIndexedIndirectCommand));
}
```
