# 🚀 Tumbler Vulkan Engine

一个现代化的 Vulkan 游戏引擎，采用基于物理的渲染 (PBR) 和实体-组件系统架构。

> **当前状态**：🚧 稳固地基期 (架构重构与 PBR 基础完成)

## ✨ 特性

- **现代化渲染管线**：基于 Vulkan 的高性能渲染
- **PBR 渲染**：支持 Cook-Torrance BRDF、金属工作流
- **实体-组件系统**：灵活的 ECS 变体架构
- **逻辑与渲染分离**：严格的数据流设计
- **ImGui 集成**：内置调试和编辑器工具

## 📖 快速开始

**👉 请查看 [快速入门指南](docs/00_Getting_Started.md) 了解如何搭建环境、编译并运行项目。**

## 📚 文档

完整的文档体系位于 `docs/` 目录：

| 文档 | 说明 |
|------|------|
| [文档索引](docs/INDEX.md) | 📍 所有文档的导航入口 |
| [快速入门指南](docs/00_Getting_Started.md) | 🚀 环境配置与构建 |
| [架构概览](docs/01_Architecture_Overview.md) | 🏗️ 引擎整体设计 |
| [资源管理](docs/02_Asset_Management.md) | 📦 资源加载与缓存 |
| [材质系统](docs/03_Material_System.md) | 🎨 PBR 材质系统 |
| [PBR 理论与实践](docs/04_PBR_Theory_and_Practice.md) | 💡 基于物理的渲染 |
| [Vulkan 核心概念](docs/05_Vulkan_Core_Concepts.md) | 🔧 Vulkan 工作流 |
| [游戏系统架构](docs/06_Game_System_Architecture.md) | 🎮 ECS 与场景管理 |
| [输入系统](docs/07_Input_System.md) | ⌨️ 键盘鼠标输入 |
| [编辑器与调试工具](docs/08_Editor_and_Debugging.md) | 🔍 ImGui 调试 |
| [渲染管线深度解析](docs/09_Rendering_Pipeline_Deep_Dive.md) | 🖼️ 渲染流程详解 |
| [故障排除指南](docs/10_Troubleshooting_Guide.md) | ⚠️ 常见问题解答 |

## 🎮 控制方式

- **WASD**: 移动相机
- **QE**: 上下移动
- **鼠标拖动**: 旋转视角
- **ImGui 面板**: 调整光源参数

## 🗺️ 开发路线图

查看 [Tumbler_Dev_Plan.md](Tumbler_Dev_Plan.md) 了解详细的开发计划。

## 🛠️ 技术栈

- **图形 API**: Vulkan 1.3+
- **语言**: C++20
- **编译器**: MSVC (Visual Studio 2022)
- **构建系统**: CMake 3.28+
- **包管理**: vcpkg

---

**Happy Rendering! 🎨**
