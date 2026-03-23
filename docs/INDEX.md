# Tumbler 引擎文档索引

欢迎使用 Tumbler 引擎文档！本索引将帮助你快速找到所需的文档。

---

## 🚀 入门指南

### [快速入门指南 (00_Getting_Started.md)](00_Getting_Started.md)
新手必读！学习如何搭建开发环境、编译并运行项目。

**内容包括：**
- 系统要求
- 依赖安装（Vulkan SDK、vcpkg、Visual Studio）
- 项目构建步骤
- 运行示例程序

---

## 📚 核心概念

### [架构概览 (01_Architecture_Overview.md)](01_Architecture_Overview.md)
了解 Tumbler 引擎的整体设计理念和架构。

**内容包括：**
- 实体-组件系统 (ECS 变体)
- 逻辑与渲染分离设计
- VulkanRenderer 子系统架构
- 材质系统架构
- 资产管理系统

### [游戏系统架构 (06_Game_System_Architecture.md)](06_Game_System_Architecture.md)
深入了解游戏世界的组织方式。

**内容包括：**
- FActor (游戏实体)
- Component (组件)
- FScene (场景管理)
- 生命周期管理
- 所有权模型

---

## 🎨 渲染系统

### [资源管理 (02_Asset_Management.md)](02_Asset_Management.md)
学习如何加载和管理 3D 资源。

**内容包括：**
- FAssetManager 的设计理念
- 自动缓存机制
- 为什么拒绝单例模式
- 依赖注入的优雅

### [材质系统 (03_Material_System.md)](03_Material_System.md)
理解 PBR 材质系统的工作原理。

**内容包括：**
- FMaterial (材质母体)
- FMaterialInstance (材质实例)
- 母体-实例模式
- Shader 中的映射

### [基于物理的渲染 (04_PBR_Theory_and_Practice.md)](04_PBR_Theory_and_Practice.md)
深入学习 PBR 渲染理论与实践。

**内容包括：**
- 核心物理法则（能量守恒、微表面理论、菲涅尔方程）
- 金属工作流参数
- Cook-Torrance BRDF
- 引擎中的代码实现

### [Vulkan 核心概念 (05_Vulkan_Core_Concepts.md)](05_Vulkan_Core_Concepts.md)
掌握 Vulkan 的关键概念和工作流。

**内容包括：**
- 核心数据结构金字塔
- VMA 显存管理
- Descriptors & Push Constants
- 渲染管线对象
- 命令池与命令缓冲
- 同步机制

### [渲染管线深度解析 (09_Rendering_Pipeline_Deep_Dive.md)](09_Rendering_Pipeline_Deep_Dive.md)
从数据准备到像素输出的完整流程剖析。

**内容包括：**
- 数据准备阶段（RenderPacket、SceneViewData）
- 渲染循环详解
- 命令缓冲录制
- 描述符集设计
- 着色器流程
- 同步机制详解
- 性能优化策略

---

## 🎮 游戏功能

### [输入系统 (07_Input_System.md)](07_Input_System.md)
学习如何处理键盘和鼠标输入。

**内容包括：**
- 系统概述
- 初始化与帧更新
- 轴输入 (Axis Input)
- 动作输入 (Action Input)
- 鼠标输入
- UI 穿透检测
- 完整示例

### [编辑器与调试工具 (08_Editor_and_Debugging.md)](08_Editor_and_Debugging.md)
掌握 ImGui 调试和编辑器工具的使用。

**内容包括：**
- 内置编辑器面板（性能统计、光源、相机、场景层级、材质编辑器）
- AppLogic 编辑器架构
- ImGui 简介
- UIManager 类使用
- 常用 ImGui 控件
- 实际示例
- 性能分析
- 调试技巧
- UpdateUBO() vs ApplyChanges()

---

## 🔧 参考文档

### [代码导航指南 (12_Code_Navigation_Guide.md)](12_Code_Navigation_Guide.md)
**新！** 帮助你快速理解代码结构，找到需要阅读的代码。

**内容包括：**
- 目录结构概览
- 按功能查找代码（渲染、游戏系统、材质、编辑器、Vulkan）
- 关键代码位置速查
- 推荐阅读顺序
- 阅读技巧

### [故障排除指南 (10_Troubleshooting_Guide.md)](10_Troubleshooting_Guide.md)
遇到问题？在这里寻找解决方案。

**内容包括：**
- 构建问题
- 运行时问题
- 资源问题
- 性能问题
- 调试工具使用
- 常见 Vulkan 错误码速查

### [开发路线图](../Tumbler_Dev_Plan.md)
查看引擎的未来发展计划。

---

## 📖 推荐阅读顺序

如果你是第一次接触 Tumbler 引擎，建议按以下顺序阅读：

1. **[快速入门指南](00_Getting_Started.md)** - 先跑起来！
2. **[架构概览](01_Architecture_Overview.md)** - 了解整体设计
3. **[代码导航指南](12_Code_Navigation_Guide.md)** - **新！** 知道去哪里找代码
4. **[游戏系统架构](06_Game_System_Architecture.md)** - 学习如何组织游戏世界
5. **[Vulkan 核心概念](05_Vulkan_Core_Concepts.md)** - 掌握底层渲染基础
6. **[材质系统](03_Material_System.md)** - 理解 PBR 材质
7. **[基于物理的渲染](04_PBR_Theory_and_Practice.md)** - 深入 PBR 理论
8. **[输入系统](07_Input_System.md)** - 添加交互
9. **[编辑器与调试工具](08_Editor_and_Debugging.md)** - 学会调试
10. **[渲染管线深度解析](09_Rendering_Pipeline_Deep_Dive.md)** - 精通渲染流程
11. **[故障排除指南](10_Troubleshooting_Guide.md)** - 遇到问题时查阅

---

## 🤝 贡献文档

发现文档有误或有遗漏？欢迎通过以下方式贡献：
- 提交 Issue 指出问题
- Fork 仓库并提交 Pull Request
- 在讨论区分享你的使用经验

---

## 📚 外部资源

- [Vulkan 官方规范](https://www.khronos.org/registry/vulkan/)
- [Vulkan Tutorial](https://vulkan-tutorial.com/)
- [Learn OpenGL](https://learnopengl.com/)
- [ImGui 文档](https://github.com/ocornut/imgui)
- [PBR 理论指南](https://learnopengl.com/PBR/Theory)
