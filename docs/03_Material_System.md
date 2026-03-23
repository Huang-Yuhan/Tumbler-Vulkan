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

## 4. 材质参数 API

### 纹理参数
```cpp
// 设置基础颜色贴图
matInstance->SetTexture("BaseColorMap", baseColorTexture);

// 设置法线贴图
matInstance->SetTexture("NormalMap", normalMapTexture);
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
