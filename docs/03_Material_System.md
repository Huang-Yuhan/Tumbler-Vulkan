# PBR 材质系统设计 (PBR Material System)

现代引擎中的材质从来不是单个文件的概念，它是一组管线状态集 (`Pipeline`)、排列 (`Layout`) 以及具体数值属性的结合体。为了平衡高性能与表现力，本项目将材质拆分为 **母体 (`FMaterial`)** 和 **实例 (`FMaterialInstance`)** 两个层级。

## 1. FMaterial (材质母体 / Material Template)
`FMaterial` 负责最底层、最沉重的 Vulkan 工作。
- 接收 `Vertex Shader` 和 `Fragment Shader`，编译为 Shader Modules。
- 根据 Shader 规定，向 Vulkan 申请 `VkDescriptorSetLayout` 和 `VkPipelineLayout`（指定内存的捆绑方式）。
- 设置剔除、混合、深度测试等光栅化状态。
- 调用 `vkCreateGraphicsPipelines`，这通常是极慢的 API 调用。

**使用原则**：它像是一张“蓝图”。在游戏中，哪怕你有 1000 个不同颜色的金属球，你只需要 **1个** `FMaterial` 实例（存放在 `AssetManager` 内）。

## 2. FMaterialInstance (材质实例 / Material Instance)
`FMaterialInstance` 从 `FMaterial` 衍生而来，代表游戏中具体某个物体最终在屏幕上呈现的独特外观。
- 它持有一个 `VkDescriptorSet` （可以理解为一张表格，填入了具体的贴图指针和纯粹的数字 Buffer）。
- 它在内部利用 VMA 申请了一块极小的 `UBO`（Uniform Buffer Object）内存，用于存放 `Roughness`, `Metallic`, `BaseColorTint` 等参数。
- 当改变参数（例如剑随着磨损变糙），只需调用 `matInstance->ApplyChanges()`，数据会被 `memcpy` 送给 GPU 前端。

**使用原则**：每一个不同表现的 Actor 都持有独立的 `FMaterialInstance` 指针。

## 3. Shader 中的映射
在 Vulkan GLSL 中：
- `Push Constants`（全局通用）通常用来传递快速变化的矩阵（`ModelMatrix`）。
- **Set 0, Binding 0**: 全局场景数据 (ViewProj, CameraPos, LightPos 等)
- **Set 1, Binding 0**: BaseColorMap (基础颜色贴图)
- **Set 1, Binding 1**: NormalMap (法线贴图)
- **Set 1, Binding 2**: Material UBO (包含所有 PBR 参数的 Uniform Buffer)
- **Set 1, Binding 3**: IrradianceMap (漫反射环境光，Cube Map)
- **Set 1, Binding 4**: PrefilterMap (镜面反射环境光，带 Mipmap 的 Cube Map)
- **Set 1, Binding 5**: BRDFLUT (BRDF 查找表，2D 贴图)

## 4. 材质参数 API

### 纹理参数
```cpp
// 设置基础颜色贴图
matInstance->SetTexture("BaseColorMap", baseColorTexture);

// 设置法线贴图
matInstance->SetTexture("NormalMap", normalMapTexture);

// --- IBL 相关纹理 ---
// 设置 Irradiance Map (漫反射环境光)
matInstance->SetTexture("IrradianceMap", irradianceMap);

// 设置 Prefilter Map (镜面反射环境光)
matInstance->SetTexture("PrefilterMap", prefilterMap);

// 设置 BRDF LUT (查找表)
matInstance->SetTexture("BRDFLUT", brdfLUT);
```

### 数值参数
```cpp
// 设置基础颜色染色
matInstance->SetVector("BaseColorTint", glm::vec4(1.0f, 0.5f, 0.5f, 1.0f));

// 设置粗糙度 (0.0 ~ 1.0)
matInstance->SetFloat("Roughness", 0.3f);

// 设置金属度 (0.0 ~ 1.0)
matInstance->SetFloat("Metallic", 1.0f);

// 设置法线贴图强度 (0.0 ~ 2.0, 默认 1.0)
matInstance->SetFloat("NormalMapStrength", 1.5f);

// 应用所有更改到 GPU
matInstance->ApplyChanges();
```

## 5. Fallback 机制

在没有给出自定义贴图时，`FMaterialInstance` 巧妙地通过反向查询 `AssetManager->GetOrLoadTexture("DefaultWhite")`，使用单张极其细小的纯白图来填充：
- **BaseColorMap**: 使用白色贴图，配合 `BaseColorTint` 实现纯颜色
- **NormalMap**: 使用白色贴图，Shader 会检测并自动回退到几何法线

这保证了 Vulkan 指针校验不会崩溃，同时兼顾了纯颜色染色的工作流。

## 6. UpdateUBO() vs ApplyChanges()

Tumbler 引擎提供了两种更新材质的方法，根据使用场景选择合适的方法：

### 6.1 UpdateUBO() - 快速更新（推荐用于编辑器）

```cpp
// 只更新 UBO 数据，不重新绑定描述符
matInstance->SetFloat("Roughness", 0.5f);
matInstance->UpdateUBO();
```

**特点：**
- ✅ 仅将 CPU 数据拷贝到持久映射的 UBO 内存
- ✅ 不调用 `vkUpdateDescriptorSets()`
- ✅ 性能更好，适合频繁调整
- ✅ 避免 Vulkan 验证错误（描述符集在使用中时不会被更新）

**适用场景：**
- 编辑器中实时调整材质参数
- 每帧都可能变化的参数
- 不需要切换纹理时

### 6.2 ApplyChanges() - 完整更新

```cpp
// 完整更新，包括重新绑定描述符
matInstance->SetTexture("BaseColorMap", newTexture);
matInstance->ApplyChanges();
```

**特点：**
- ✅ 更新 UBO 数据
- ✅ 重新绑定所有纹理描述符
- ✅ 调用 `vkUpdateDescriptorSets()`
- ⚠️ 可能导致 Vulkan 验证错误（如果描述符集正在被 GPU 使用）

**适用场景：**
- 初始化材质时
- 需要切换纹理时
- 描述符集布局改变时

### 6.3 最佳实践

| 操作 | 推荐方法 |
|------|----------|
| 调整 Roughness/Metallic/BaseColor | `UpdateUBO()` |
| 调整 NormalStrength/TwoSided | `UpdateUBO()` |
| 切换 BaseColorMap/NormalMap | `ApplyChanges()` |
| 初始化新材质实例 | `ApplyChanges()` |
| 编辑器中实时预览 | `UpdateUBO()` |

**重要提示：** 在编辑器中使用 `UpdateUBO()` 可以避免 Vulkan 验证层报错！详见 [编辑器与调试工具](08_Editor_and_Debugging.md) 文档。
