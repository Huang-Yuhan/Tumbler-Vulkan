# 关键设计决策记录

## 为什么双管线（Forward + Deferred）而不是只做 Deferred？

Forward 和 Deferred 各有优势：

| | Forward | Deferred |
|------|---------|----------|
| 多光源性能 | O(物体 × 光源) | O(物体) + O(光源 × 像素) |
| 透明物体 | 天然支持 | 需要额外 pass |
| 内存带宽 | 低 | 高（G-Buffer 写入） |
| MSAA | 简单 | 复杂（需额外 resolve） |
| 简单场景 | 更快 | G-Buffer 开销无必要 |

引擎同时保留两条管线，允许根据场景需求运行时切换。默认 Forward 适合简单场景和透明渲染，Deferred 适合多光源复杂场景。

---

## 为什么 ECS 变体（FActor 持有 CTransform）而不是纯 ECS？

纯 ECS（所有数据在数组中按类型连续存储）在 Cache 友好的批量遍历上有优势，但对于当前引擎的规模（< 100 个 Actor），维护纯 ECS 的复杂度不值得。

FActor 将 CTransform 作为值成员（避免额外堆分配），其他组件通过 `unique_ptr` 持有。这个折中方案：
- 避免了纯 ECS 的数据重排布复杂度
- Actor 之间的 Transform 层级关系表达自然
- 组件行为组合足够灵活

---

## 为什么 VMA 而不是手动 VkDeviceMemory？

Vulkan Memory Allocator (VMA) 由 AMD 维护，在 Vulkan 社区中是事实标准。手动管理 VkDeviceMemory：
- 受限于 `maxMemoryAllocationCount`（通常 4096）
- 需要手动处理子分配、碎片整理、内存类型选择
- 需要为不同用途（staging vs device-local）手动选择堆

VMA 提供 `VMA_MEMORY_USAGE_AUTO_PREFER_*` 策略，自动处理上述问题，且零额外成本（header-only）。

---

## 为什么渲染与逻辑分离（RenderPacket + SceneViewData）？

`VulkanRenderer::Render()` 只接收 `vector<RenderPacket>` 和 `SceneViewData`，对 Actor/Scene/Component 一无所知。这个决策的核心原因：

1. **无法反向依赖**：渲染器不知道 Scene 的存在，确保逻辑层和渲染层的解耦是物理强制的
2. **多线程友好**：提取 RenderPacket 是只读操作，可以并行执行；渲染器消费数据是无副作用的
3. **管线无关**：Forward 和 Deferred 管线接收同一份数据，切换管线不需要修改游戏逻辑
4. **易于测试**：可以构造假的 RenderPacket 列表直接测试渲染，不需要启动完整场景

---

## 为什么母体-实例材质模式？

`FMaterial` 持有不可变的管线资源（VkPipeline、VkPipelineLayout、VkDescriptorSetLayout），这些创建成本高、数量少。`FMaterialInstance` 持有可变参数（UBO + DescriptorSet），创建成本低、数量多。

这个模式的收益：
- 多个物体共享同一个材质母体时，只需 Bake 一次管线
- 每个实例可以独立调整颜色、粗糙度等参数
- 材质母体在两种管线（Forward + Deferred）下各编译一份，实例自动适应

---

## 为什么 GLFW 而不是 SDL2 / 纯 Win32+X11？

- GLFW 对 Vulkan 的原生支持最成熟（`glfwCreateWindowSurface` 直接接受 `VkInstance`）
- 项目需要 `glfwGetPlatform` 等新 API（3.4+）
- vcpkg 的 GLFW 版本支持 Wayland + X11 双后端，Linux 兼容性好
- SDL2 的 Vulkan 支持较晚加入，能力不如 GLFW 全面
