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

## 技术参考

| 文档 | 内容 |
|------|------|
| [着色器参考](reference/shaders.md) | Shader 文件清单、描述符绑定点、G-Buffer 布局 |
| [Vulkan 核心概念](reference/vulkan-concepts/) | 基础设施 / 资源与内存 / 管线与同步 |
| [PBR 理论与实现](reference/pbr-theory.md) | Cook-Torrance BRDF、金属工作流 |
| [基于图像的照明](reference/ibl.md) | IBL 理论与引擎集成 |

## 运维

| 文档 | 内容 |
|------|------|
| [故障排除](troubleshooting.md) | 构建、运行时、Linux/Wayland 问题 |
| [测试与 CI](testing-and-ci.md) | 测试结构、CTest 使用、CI 流程 |

---

## 推荐阅读顺序

1. [环境搭建](getting-started/setup.md) — 先跑起来
2. [架构概览](architecture/overview.md) — 理解 GPU-Driven 整体设计
3. [渲染架构](architecture/rendering.md) — 理解帧循环和 GPU 数据流
4. [设计决策](architecture/decisions.md) — 理解为什么这样设计
5. [着色器参考](reference/shaders.md) — 理解 Shader 绑定点
6. 其余按需查阅
