# Tumbler 引擎文档索引

## 入门

| 文档 | 内容 |
|------|------|
| [环境搭建](getting-started/setup.md) | 系统要求、依赖安装、构建运行 |

## 架构

| 文档 | 内容 |
|------|------|
| [架构概览](architecture/overview.md) | GPU-Driven 设计原则、分层结构、数据流 |
| [设计决策](architecture/decisions.md) | 为什么选择 GPU-Driven？关键取舍记录 |
| [渲染架构](architecture/rendering.md) | Compute Culling、Indirect Draw、Deferred 管线、Bindless |
| [ECS 系统](architecture/ecs.md) | FActor / FScene / Component 设计 |
| [材质系统](architecture/material.md) | Material 数据流与参数编辑 |

## 开发指南

| 文档 | 内容 |
|------|------|
| [代码导航](code-navigation.md) | 文件级速查表，按功能定位代码 |
| [编码规范](standards/coding-style.md) | 命名规则、日志、架构约束、Vulkan 约定 |
| [GPU-Driven 开发计划](gpu-driven-dev-plan.md) | Nanite 渲染器 Phase 4-10 详细规划 |
| [后续开发指南](guides/continuation.md) | 当前进度、待做任务、新机环境搭建 |
| [资产管线](guides/assets.md) | 资产导入流程、文件格式说明 |
| [编辑器与调试](guides/editor.md) | ImGui 面板、控制台使用 |
| [输入系统](guides/input.md) | InputManager 绑定与配置 |

## 技术参考

| 文档 | 内容 |
|------|------|
| [数学工具](reference/math.md) | 视锥体提取、常用运算 |
| [着色器参考](reference/shaders.md) | Shader 绑定点、G-Buffer 布局 |
| [渲染管线](reference/rendering-pipeline.md) | 帧循环、同步细节 |
| [Vulkan 核心概念](reference/vulkan-concepts/) | 基础设施 / 资源与内存 / 管线与同步 |
| [PBR 理论与实现](reference/pbr-theory.md) | Cook-Torrance BRDF、金属工作流 |
| [基于图像的照明](reference/ibl.md) | IBL 理论与引擎集成 |

## UE5 源码调研

| 文档 | 内容 |
|------|------|
| [调研索引](ue-research/INDEX.md) | 调研主题列表 + UE 源文件速查 |
| [Nanite 材质系统](ue-research/nanite-material-system.md) | 多材质架构、FPackedCluster 编码、RasterBin/ShadeBin 分发 |

## 运维

| 文档 | 内容 |
|------|------|
| [机器配置](machine-setups.md) | 各开发机的工具链路径与环境变量 |
| [故障排除](troubleshooting.md) | 构建、运行时、Linux/Wayland 问题 |
| [测试与 CI](testing-and-ci.md) | 测试结构、CTest 使用、CI 流程 |

---

## 推荐阅读顺序

1. [环境搭建](getting-started/setup.md) — 先跑起来
2. [机器配置](machine-setups.md) — 配置本机工具链路径
3. [代码导航](code-navigation.md) — 快速定位代码位置
4. [编码规范](standards/coding-style.md) — 了解项目约定
5. [架构概览](architecture/overview.md) — 理解整体设计
5. [GPU-Driven 开发计划](gpu-driven-dev-plan.md) — 了解后续方向
6. [后续开发指南](guides/continuation.md) — 当前进度
7. 其余按需查阅
