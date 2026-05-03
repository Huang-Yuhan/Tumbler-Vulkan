# Tumbler 引擎文档索引

## 🚀 入门

| 文档 | 内容 |
|------|------|
| [环境搭建](getting-started/setup.md) | 系统要求、依赖安装、构建运行 |

## 🏗️ 架构

| 文档 | 内容 |
|------|------|
| [架构概览](architecture/overview.md) | 整体设计原则、分层结构、数据流 |
| [设计决策](architecture/decisions.md) | 为什么这样设计？关键取舍记录 |
| [渲染架构](architecture/rendering.md) | 双管线策略、Deferred 优化、UI 解耦 |
| [ECS 游戏系统](architecture/ecs.md) | FActor / Component / FScene 生命周期 |
| [材质系统](architecture/material.md) | 母体-实例模式、PBR 材质参数 |

## 📖 使用指南

| 文档 | 内容 |
|------|------|
| [编辑器与调试](guides/editor.md) | ImGui 面板、材质编辑器、性能分析 |
| [运行时控制台](guides/console.md) | 命令参考、Tab 补全、自定义命令注册 |
| [输入系统](guides/input.md) | 轴绑定、动作绑定、UI 穿透处理 |
| [资产管理](guides/assets.md) | 模型/纹理/材质加载与缓存 |

## 📚 技术参考

| 文档 | 内容 |
|------|------|
| [渲染管线深度解析](reference/rendering-pipeline.md) | 帧循环、G-Buffer、同步细节 |
| [着色器参考](reference/shaders.md) | 着色器文件清单、绑定点、G-Buffer 布局 |
| [Vulkan 核心概念](reference/vulkan-concepts/) | 分三篇：核心基础设施 / 资源与内存 / 管线与同步 |
| [PBR 理论与实现](reference/pbr-theory.md) | Cook-Torrance BRDF、金属工作流 |
| [基于图像的照明](reference/ibl.md) | IBL 理论与引擎集成 |

## 🛠️ 运维

| 文档 | 内容 |
|------|------|
| [故障排除](troubleshooting.md) | 构建、运行时、Linux/Wayland 问题 |
| [测试与 CI](testing-and-ci.md) | 测试结构、CTest 使用、CI 流程 |
| [代码导航](code-navigation.md) | 功能 → 文件速查表 |

---

## 推荐阅读顺序

1. [环境搭建](getting-started/setup.md) — 先跑起来
2. [架构概览](architecture/overview.md) — 理解整体设计
3. [设计决策](architecture/decisions.md) — 理解为什么这样设计
4. [代码导航](code-navigation.md) — 知道去哪找代码
5. [ECS 游戏系统](architecture/ecs.md) — 理解游戏世界结构
6. [渲染架构](architecture/rendering.md) — 理解双管线策略
7. [渲染管线深度解析](reference/rendering-pipeline.md) — 深入渲染细节
8. [编辑器与调试](guides/editor.md) — 学会调试
9. 其余按需查阅
