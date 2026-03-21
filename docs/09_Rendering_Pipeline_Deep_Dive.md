# 渲染管线深度解析 (Rendering Pipeline Deep Dive)

本文档深入剖析 Tumbler 引擎的 Vulkan 渲染管线，从数据准备到最终像素输出的完整流程。

## 1. 渲染架构概览

Tumbler 引擎的渲染系统采用严格的分层设计：

```
┌─────────────────────────────────────────────────────────┐
│                    应用层 (AppLogic)                     │
│  - 场景管理 (FScene)                                    │
│  - 输入处理                                              │
│  - 游戏逻辑                                              │
└────────────────────┬────────────────────────────────────┘
                     │ 数据
                     ▼
┌─────────────────────────────────────────────────────────┐
│              数据打包层 (Data Collection)                │
│  - ExtractRenderPackets()                                │
│  - GenerateSceneView()                                   │
│  - RenderPacket + SceneViewData                          │
└────────────────────┬────────────────────────────────────┘
                     │ 纯净数据
                     ▼
┌─────────────────────────────────────────────────────────┐
│               渲染层 (VulkanRenderer)                    │
│  - 命令缓冲录制                                          │
│  - 管线绑定                                              │
│  - 绘制调用                                              │
└────────────────────┬────────────────────────────────────┘
                     │ Vulkan 命令
                     ▼
┌─────────────────────────────────────────────────────────┐
│                   驱动与 GPU                             │
│  - 执行命令缓冲                                          │
│  - 光栅化                                                │
│  - 像素着色                                              │
└─────────────────────────────────────────────────────────┘
```

## 2. 数据准备阶段

### 2.1 RenderPacket (渲染包)

`RenderPacket` 是从场景中提取的纯净渲染数据，不包含任何游戏逻辑：

```cpp
struct RenderPacket {
    FMesh* Mesh;                    // 几何体
    FMaterialInstance* Material;    // 材质实例
    glm::mat4 ModelMatrix;          // 模型矩阵
};
```

### 2.2 SceneViewData (场景视图数据)

`SceneViewData` 代表特定相机视角下的环境信息：

```cpp
struct SceneViewData {
    glm::mat4 View;           // 视图矩阵
    glm::mat4 Proj;           // 投影矩阵
    glm::mat4 ViewProj;       // 视图投影矩阵
    glm::vec3 CameraPosition; // 相机位置
    std::vector<LightData> Lights; // 可见光源列表
};
```

### 2.3 数据提取流程

```cpp
// 1. 从场景中提取所有可渲染物体
std::vector<RenderPacket> renderPackets;
Scene->ExtractRenderPackets(renderPackets);

// 2. 为特定相机生成视图数据
SceneViewData viewData = Scene->GenerateSceneView(
    camera,
    &camera->GetOwner()->Transform,
    aspectRatio
);

// ExtractRenderPackets 内部实现：
void FScene::ExtractRenderPackets(std::vector<RenderPacket>& outPackets) const {
    for (const auto& actor : Actors) {
        if (auto* renderer = actor->GetComponent<CMeshRenderer>()) {
            RenderPacket packet;
            packet.Mesh = renderer->GetMesh();
            packet.Material = renderer->GetMaterial();
            packet.ModelMatrix = actor->Transform.GetMatrix();
            outPackets.push_back(packet);
        }
    }
}
```

## 3. 渲染循环详解

### 3.1 完整帧流程

```cpp
void VulkanRenderer::Render(
    const SceneViewData& viewData,
    const std::vector<RenderPacket>& renderPackets,
    std::function<void(VkCommandBuffer)> onUIRender
) {
    // ==========================================
    // 阶段 1: 等待上一帧完成
    // ==========================================
    vkWaitForFences(GetDevice(), 1, &RenderFence, VK_TRUE, UINT64_MAX);
    vkResetFences(GetDevice(), 1, &RenderFence);

    // ==========================================
    // 阶段 2: 获取交换链图像
    // ==========================================
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        GetDevice(),
        SwapChain.GetSwapchain(),
        UINT64_MAX,
        ImageAvailableSemaphore,
        VK_NULL_HANDLE,
        &imageIndex
    );

    // 处理窗口重置
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchain();
        return;
    }

    // ==========================================
    // 阶段 3: 重置命令缓冲
    // ==========================================
    vkResetCommandBuffer(MainCommandBuffer, 0);

    // ==========================================
    // 阶段 4: 录制命令
    // ==========================================
    RecordCommandBuffer(
        MainCommandBuffer,
        imageIndex,
        viewData,
        renderPackets,
        onUIRender
    );

    // ==========================================
    // 阶段 5: 提交命令队列
    // ==========================================
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore waitSemaphores[] = { ImageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &MainCommandBuffer;
    
    VkSemaphore signalSemaphores[] = { RenderFinishedSemaphore };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    
    vkQueueSubmit(GetGraphicsQueue(), 1, &submitInfo, RenderFence);

    // ==========================================
    // 阶段 6: 呈现图像
    // ==========================================
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    
    VkSwapchainKHR swapChains[] = { SwapChain.GetSwapchain() };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    
    vkQueuePresentKHR(GetGraphicsQueue(), &presentInfo);
}
```

## 4. 命令缓冲录制

### 4.1 录制流程详解

```cpp
void VulkanRenderer::RecordCommandBuffer(
    VkCommandBuffer cmd,
    uint32_t imageIndex,
    const SceneViewData& viewData,
    const std::vector<RenderPacket>& renderPackets,
    std::function<void(VkCommandBuffer)> onUIRender
) {
    // ==========================================
    // 步骤 1: 开始命令缓冲
    // ==========================================
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // ==========================================
    // 步骤 2: 更新全局数据
    // ==========================================
    UpdateSceneParameterBuffer(viewData);

    // ==========================================
    // 步骤 3: 开始渲染通道
    // ==========================================
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = RenderPass;
    renderPassInfo.framebuffer = Framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = GetSwapchainExtent();
    
    VkClearValue clearValues[2];
    clearValues[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues;
    
    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // ==========================================
    // 步骤 4: 设置视口和裁剪
    // ==========================================
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)GetSwapchainExtent().width;
    viewport.height = (float)GetSwapchainExtent().height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = GetSwapchainExtent();
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // ==========================================
    // 步骤 5: 绑定全局描述符集 (Set 0)
    // ==========================================
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pbrMaterial->GetPipelineLayout(),
        0, 1, &GlobalDescriptorSet,
        0, nullptr
    );

    // ==========================================
    // 步骤 6: 绘制所有物体
    // ==========================================
    for (const auto& packet : renderPackets) {
        DrawRenderPacket(cmd, packet);
    }

    // ==========================================
    // 步骤 7: 绘制 UI
    // ==========================================
    if (onUIRender) {
        onUIRender(cmd);
    }

    // ==========================================
    // 步骤 8: 结束渲染通道和命令缓冲
    // ==========================================
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}
```

### 4.2 单个物体绘制

```cpp
void VulkanRenderer::DrawRenderPacket(VkCommandBuffer cmd, const RenderPacket& packet) {
    // 1. 获取 GPU 端网格
    FVulkanMesh& vulkanMesh = UploadMesh(packet.Mesh);
    
    // 2. 绑定管线
    FMaterial* material = packet.Material->GetParentMaterial();
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, material->GetPipeline());
    
    // 3. 绑定材质描述符集 (Set 1)
    VkDescriptorSet matSet = packet.Material->GetDescriptorSet();
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        material->GetPipelineLayout(),
        1, 1, &matSet,
        0, nullptr
    );
    
    // 4. Push Constants (Model 矩阵)
    vkCmdPushConstants(
        cmd,
        material->GetPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT,
        0, sizeof(glm::mat4),
        &packet.ModelMatrix
    );
    
    // 5. 绑定顶点/索引缓冲
    VkBuffer vertexBuffers[] = { vulkanMesh.VertexBuffer.Buffer };
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, vulkanMesh.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
    
    // 6. 绘制!
    vkCmdDrawIndexed(cmd, vulkanMesh.IndexCount, 1, 0, 0, 0);
}
```

## 5. 描述符集设计

### 5.1 描述符布局

Tumbler 引擎采用双描述符集设计，按更新频率分组：

```
Set 0 (全局，每帧更新一次):
├─ Binding 0: SceneData UBO
│  ├─ ViewProj 矩阵
│  ├─ CameraPosition
│  └─ Lights[]
└─ Binding 1: 天空盒纹理 (预留)

Set 1 (材质，每材质切换时更新):
├─ Binding 0: Albedo 纹理
├─ Binding 1: Normal 纹理 (预留)
├─ Binding 2: MaterialParams UBO
│  ├─ BaseColorTint
│  ├─ Roughness
│  └─ Metallic
└─ ... 更多纹理
```

### 5.2 全局描述符更新

```cpp
void VulkanRenderer::UpdateSceneParameterBuffer(const SceneViewData& viewData) {
    // 将 SceneViewData 映射到 UBO 结构
    struct SceneUBO {
        glm::mat4 ViewProj;
        glm::vec3 CameraPos;
        // ... 光源数据
    };
    
    SceneUBO ubo;
    ubo.ViewProj = viewData.ViewProj;
    ubo.CameraPos = viewData.CameraPosition;
    
    // 复制到 GPU
    void* data;
    vmaMapMemory(Allocator, SceneParameterBuffer.Allocation, &data);
    memcpy(data, &ubo, sizeof(SceneUBO));
    vmaUnmapMemory(Allocator, SceneParameterBuffer.Allocation);
}
```

## 6. 着色器流程

### 6.1 顶点着色器 (pbr.vert)

```glsl
#version 450

// Push Constants (每一物体更新)
layout(push_constant) uniform PushConstants {
    mat4 Model;
} pushConsts;

// Set 0 (全局，每帧更新)
layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 ViewProj;
    vec3 CameraPos;
} scene;

// 顶点输入
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

// 顶点输出
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

void main() {
    // 计算世界坐标
    vec4 worldPos = pushConsts.Model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    
    // 变换法线 (法线矩阵)
    fragNormal = mat3(transpose(inverse(pushConsts.Model))) * inNormal;
    
    // 传递纹理坐标
    fragTexCoord = inTexCoord;
    
    // 最终裁剪空间坐标
    gl_Position = scene.ViewProj * worldPos;
}
```

### 6.2 片段着色器 (pbr.frag)

```glsl
#version 450

// Set 0 (全局)
layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 ViewProj;
    vec3 CameraPos;
    // ... 光源
} scene;

// Set 1 (材质)
layout(set = 1, binding = 0) uniform sampler2D albedoTex;
layout(set = 1, binding = 1) uniform MaterialUBO {
    vec4 BaseColorTint;
    float Roughness;
    float Metallic;
} mat;

// 片段输入
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

// 片段输出
layout(location = 0) out vec4 outColor;

// PBR 函数 (Distribution GGX, Fresnel Schlick, Geometry Smith)
// ... [详见 PBR 文档]

void main() {
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(scene.CameraPos - fragWorldPos);
    
    // 采样 Albedo
    vec3 albedo = texture(albedoTex, fragTexCoord).rgb;
    albedo *= mat.BaseColorTint.rgb;
    
    // 计算光照 (Cook-Torrance BRDF)
    vec3 Lo = vec3(0.0);
    // ... [对每个光源计算光照贡献]
    
    // 最终颜色 + Gamma 校正
    vec3 color = Lo + vec3(0.03) * albedo; // 环境光
    color = color / (color + vec3(1.0));    // Tone mapping
    color = pow(color, vec3(1.0/2.2));      // Gamma correction
    
    outColor = vec4(color, 1.0);
}
```

## 7. 同步机制详解

### 7.1 信号量流程

```
CPU Timeline:
Frame N-1 ──────────────────────────────────────┐
                                                  │
Frame N:  vkWaitForFences ──┐                  │
                              │                  │
             vkAcquireNextImage(imageAvailable) │
                              │                  │
             RecordCommands   │                  │
                              │                  │
             vkQueueSubmit(  │                  │
               wait: imageAvailable,            │
               signal: renderFinished,          │
               fence: renderFence)              │
                              │                  │
             vkQueuePresent(  │                  │
               wait: renderFinished)            │
                              │                  │
                              └──────────────────┘
```

### 7.2 Fence 与信号量的区别

| 特性 | Fence | Semaphore |
|------|-------|-----------|
| 同步范围 | CPU ↔ GPU | GPU ↔ GPU |
| 信号方式 | 手动重置 | 自动消耗 |
| 等待方式 | vkWaitForFences | vkQueueSubmit / vkQueuePresent |
| 用途 | CPU 等待 GPU 完成 | 队列间操作同步 |

## 8. 性能优化策略

### 8.1 描述符集频率分组

| Set | 内容 | 更新频率 | 优势 |
|-----|------|----------|------|
| Set 0 | 摄像机、光源 | 每帧一次 | 只绑定一次 |
| Set 1 | 材质参数、纹理 | 每材质一次 | 减少切换开销 |

### 8.2 命令缓冲重用

- **不**: 每帧分配/释放命令缓冲
- **是**: 初始化时分配，每帧重置重用

### 8.3 Mesh 缓存

```cpp
class ResourceUploadManager {
    std::unordered_map<FMesh*, FVulkanMesh> MeshCache;
    
    FVulkanMesh& UploadMesh(FMesh* cpuMesh) {
        auto it = MeshCache.find(cpuMesh);
        if (it != MeshCache.end()) {
            return it->second; // 缓存命中，直接返回
        }
        
        // 缓存未命中，上传新 Mesh
        FVulkanMesh gpuMesh = UploadToGPU(cpuMesh);
        MeshCache[cpuMesh] = std::move(gpuMesh);
        return MeshCache[cpuMesh];
    }
};
```

## 9. 调试与验证

### 9.1 启用 Vulkan 验证层

在 Debug 模式下，确保验证层已启用：

```cpp
const char* validationLayers[] = {
    "VK_LAYER_KHRONOS_validation"
};
```

### 9.2 常见验证错误

| 错误 | 原因 | 解决 |
|------|------|------|
| `VUID-vkFreeCommandBuffers-pCommandBuffers-00047` | 命令缓冲还在使用中 | 用 Fence 等待或重用 |
| `VUID-vkCmdBindDescriptorSets-parameter` | 描述符集布局不匹配 | 确保 Pipeline 与 DescriptorSet 布局一致 |
| `VUID-vkCmdDrawIndexed-indexBuffer-00497` | 索引缓冲未绑定 | 检查 `vkCmdBindIndexBuffer` |
