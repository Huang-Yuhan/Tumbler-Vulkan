# 后续开发指南

在另一台机器上继续 Tumbler 引擎开发的完整说明。

---

## 1. 当前状态总览

**分支**: `nanite-integration` | **提交数**: 7 | **测试**: 92/92 通过

### 已完成的模块

| 阶段 | 内容 | 关键文件 |
|------|------|----------|
| Phase 1 | Math 库 + AppWindow + Log | `src/Core/Math/`, `src/Core/Platform/`, `src/Core/Utils/Log` |
| Phase 2 | Vulkan 1.4 基础设施 | `VulkanContext`, `RenderDevice` (VMA), `CommandManager` |
| Phase 3 | 交换链 + 资源 + 描述符 | `VulkanSwapchain`, `ResourceManager`, `DescriptorManager` |
| Part 2 | 资产导入 CLI | `src/Tools/AssetImporter/` — `TumblerImporter` 可执行文件 |
| Part 3 | 运行时资产映射 | `src/Core/Assets/AssetDatabase` |
| Part 4 | Scene 加载 + 组件 | `src/Core/Scene/SceneLoader`, `CStaticMesh`/`CCamera`/`CPointLight`/`CDirectionalLight` |
| Part 5 | Engine 编排 | `src/Core/Engine/Engine` + `EngineConfig` |

### 架构

```
Engine (生命周期)
  ├── EngineConfig (engine.json)
  ├── AssetDatabase (asset_map.json → 源路径→cooked 映射)
  ├── AppWindow (GLFW + Surface)
  ├── VulkanContext (Instance + Device 1.4)
  ├── RenderDevice (VMA)
  ├── CommandManager (CommandPool + ImmediateSubmit)
  ├── VulkanSwapchain (Swapchain + D32 depth)
  ├── DescriptorManager (Set 0 Global + Set 1 Bindless)
  └── ResourceManager (统一 VB/IB + Mesh/Texture/Shader 上传)
```

ECS: `FActor` → `CTransform` + `Component[]` (TypeMap 查询)

---

## 2. 新机器环境搭建

### 2.1 克隆并切换分支

```bash
git clone git@github.com:Huang-Yuhan/Tumbler-Vulkan.git
cd Tumbler-Vulkan
git checkout nanite-integration
```

### 2.2 安装依赖

**vcpkg**:
```bash
git clone https://github.com/Microsoft/vcpkg.git ~/.vcpkg-clion/vcpkg
cd ~/.vcpkg-clion/vcpkg && ./bootstrap-vcpkg.sh
```

**Vulkan SDK 1.4** (Linux):
```bash
# 下载 LunarG SDK 1.4.xxx
# 解压到 ~/.local/vulkan-sdk/<version>/x86_64
```

### 2.3 环境变量 (Linux)

```bash
export VCPKG_ROOT=$HOME/.vcpkg-clion/vcpkg
export VULKAN_SDK=$HOME/.local/vulkan-sdk/1.4.350.1/x86_64
export LD_LIBRARY_PATH=$VULKAN_SDK/lib/VulkanLoader/lib:$LD_LIBRARY_PATH
export PATH=$VULKAN_SDK/bin:$PATH
```

### 2.4 配置与构建

```bash
cmake -S . -B build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DBUILD_TESTING=ON

cmake --build build-linux
```

### 2.5 运行测试

```bash
ctest --test-dir build-linux --output-on-failure
```

### 2.6 Pre-commit Hook

```bash
git config core.hooksPath .githooks
```

Hook 会自动执行：格式检查 → 编译 → 测试。

---

## 3. 待做任务

### Part 6: 示例重写（优先）

**目标**: 用新 Engine API 重写示例应用。

**当前 `src/Examples/` 状态**: 旧代码已被删除或注释。`src/Examples/CMakeLists.txt` 被注释掉。

**需要做的**:
1. 取消 `CMakeLists.txt` 中 `add_subdirectory(src/Examples)` 的注释
2. 创建新的 `src/Examples/Tumbler/main.cpp`
3. 使用 Engine API 的示例代码如下：

```cpp
#include "Core/Engine/Engine.h"
#include "Core/Engine/EngineConfig.h"
#include "Core/Scene/SceneLoader.h"
#include "Core/Assets/AssetDatabase.h"
#include "Core/GameSystem/FScene.h"

int main() {
    Tumbler::EngineConfig config;
    config.LoadFromFile("engine.json");

    Tumbler::Engine engine;
    if (!engine.Init(config)) return 1;

    // 加载场景
    Tumbler::FScene scene;
    Tumbler::SceneLoader::Result result;
    Tumbler::SceneLoader loader;
    loader.LoadFromFile(scene, "assets/scenes/demo.tscene",
                        *engine.GetAssetDatabase(), result);

    // 主循环
    engine.Run();

    engine.Shutdown();
    return 0;
}
```

4. 创建配套的 `engine.json` 和场景 JSON 文件
5. 先用 `TumblerImporter` 导入资产生成 cooked 目录

### Phase 4: VisBuffer + 单三角形 SW 光栅

**目标**: Compute Shader 光栅化单个三角形 → VisBuffer64

**新文件**:
- `src/Core/Graphics/VisBuffer.h/.cpp` — R64_UINT 纹理，ImageInterlockedMaxUInt64
- `src/Core/Graphics/RasterPass.h/.cpp` — SW 光栅 Compute Shader 调度
- `assets/shaders/engine/rasterize.comp` — 边函数 + 原子写 VisBuffer64

**步骤**:
1. 创建 VisBuffer: `R64_UINT` 格式纹理，Clear(0)
2. 写 `rasterize.comp`: 硬编码三角形顶点（裁剪空间），边函数测试，InterlockedMax 写入
3. RasterPass CPU 端: 创建 Compute Pipeline，Dispatch(1,1,1)
4. Readback 验证: 读取 VisBuffer64 像素值，验证三角形渲染正确

**依赖**: `shaderInt64` 特性（检查 `VkPhysicalDeviceShaderAtomicInt64Features`）

### Phase 5-10

后续阶段参考 `docs/gpu-driven-dev-plan.md`：
- Phase 5: Cluster 数据结构 + CPU 预处理
- Phase 6: GPU Cluster 光栅化（无 LOD）
- Phase 7: Cluster 层级 + LOD 选择
- Phase 8: HW 光栅化 + 混合光栅
- Phase 9: Deferred Material
- Phase 10: 压缩编码 + 流式加载

---

## 4. 关键约定速查

### 命名空间
- **新代码**: `namespace Tumbler` — Engine、Vulkan 模块、组件、资产系统
- **旧 ECS**: 全局命名空间 — `::FActor`, `::FScene`, `::Component`, `::CTransform`
- 在 Tumbler 代码中引用旧 ECS 类型需加 `::` 前缀

### 单例策略
**不使用单例**。Engine 用 `std::unique_ptr` 持有所有子系统，通过引用注入依赖。

### 新模块添加
- `src/Core/` 下新增 `.cpp` 被 `GLOB_RECURSE` 自动收集
- TumblerImporter 的 CMakeLists.txt 显式列举源文件
- 新增测试: 加到 `tests/CMakeLists.txt`（单元）或 `tests/unit/Graphics/`（Vulkan 测试，GLOB 自动收集）

### 资产管线
```
源文件 (OBJ/PNG) → TumblerImporter → cooked/ (.tmesh/.ttex/.tmat + asset_map.json)
                                               ↓
                                      AssetDatabase.LoadAssetMap()
                                               ↓
                                      SceneLoader → FScene + Components
```

- Scene JSON 引用源文件路径（非 cooked）
- 只提交源文件，`cooked/` 在 `.gitignore`
- 旋转用 Quaternion [x,y,z,w]
- Material Metallic-Roughness 合并通道（glTF 风格）

### 第三方库实现
- `VMAImplementations.cpp` — VMA（仅 TumblerCore，需要 Vulkan 头文件）
- `StbImplementations.cpp` — stb_image + stb_image_resize2 + tinyobjloader（TumblerCore 和 TumblerImporter 共享）

### 代码格式化
```bash
find src tests -name "*.h" -o -name "*.cpp" | xargs clang-format -i
```
配置: `.clang-format` (LLVM, 4 空格, C++20, 120 列宽)

---

## 5. 快速命令参考

```bash
# 构建
cmake --build build-linux

# 测试
ctest --test-dir build-linux -L unit          # 仅单元（无需 Vulkan）
ctest --test-dir build-linux --output-on-failure  # 全部

# 资产导入
./build-linux/src/Tools/AssetImporter/TumblerImporter mesh assets/models/demo.obj --output cooked/
./build-linux/src/Tools/AssetImporter/TumblerImporter texture assets/textures/demo.png --output cooked/
./build-linux/src/Tools/AssetImporter/TumblerImporter --input assets/scenes/demo.json --output cooked/

# 格式化
find src tests -name "*.h" -o -name "*.cpp" | xargs clang-format -i
```

---

## 6. 相关文档

- `CLAUDE.md` — 完整架构参考（刚重写）
- `docs/gpu-driven-dev-plan.md` — Nanite 渲染器开发计划
- `.claude/plans/lazy-wandering-owl.md` — 当前开发计划
- `docs/troubleshooting.md` — 常见问题
- `docs/getting-started/setup.md` — 环境搭建详解
