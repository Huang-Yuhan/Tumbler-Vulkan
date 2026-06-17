# Part 6: 示例应用骨架 — 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用新 Engine API 创建可运行的 Tumbler 示例应用，跑通 Engine 初始化 → 资产加载 → 主循环链路。

**Architecture:** `main()` 创建 EngineConfig → Engine → SceneLoader → 进入 Run() 主循环。Engine::Run() 目前只做 Acquire/Present（无渲染器），通过日志验证所有子系统正常启动。

**Tech Stack:** C++20, TumblerCore 静态库, GLFW, Vulkan 1.4, spdlog

## Global Constraints

- 所有新代码使用 `namespace Tumbler`（ECS 旧类型用 `::` 前缀）
- 不使用单例，Engine 通过引用注入依赖
- 新 `.cpp` 文件被 `GLOB_RECURSE` 自动收集进 TumblerCore
- `cooked/` 目录在 `.gitignore`，不提交
- 构建后需将 `assets/` 复制到可执行文件目录

---

### Task 1: 创建配置文件和资产 JSON

**Files:**
- Create: `engine.json`
- Create: `assets/materials/sword.tmat`
- Create: `assets/scenes/demo.tscene`

**Interfaces:**
- Produces: `engine.json` (EngineConfig 消费), `sword.tmat` (TumblerImporter 消费), `demo.tscene` (SceneLoader 消费)

- [ ] **Step 1: 创建 `engine.json`**

```json
{
    "window": {
        "width": 1280,
        "height": 720,
        "title": "Tumbler"
    },
    "render": {
        "cookedPath": "cooked/",
        "assetMap": "cooked/asset_map.json"
    }
}
```

- [ ] **Step 2: 创建 `assets/materials/sword.tmat`**

```json
{
    "name": "Sword",
    "albedo": "assets/textures/1.jpg",
    "normal": "",
    "metallicRoughness": "",
    "baseColorTint": [1.0, 1.0, 1.0],
    "roughness": 0.5,
    "metallic": 0.1
}
```

- [ ] **Step 3: 创建 `assets/scenes/demo.tscene`**

```json
{
    "name": "Demo",
    "camera": {
        "fov": 60.0,
        "nearPlane": 0.1,
        "farPlane": 1000.0,
        "position": [0.0, 2.0, 8.0],
        "lookAt": [0.0, 1.0, 0.0]
    },
    "objects": [
        {
            "name": "Sword",
            "mesh": "assets/models/Sting-Sword-lowpoly.obj",
            "materials": ["assets/materials/sword.tmat"],
            "transform": {
                "position": [0.0, 1.5, 0.0],
                "scale": [1.0, 1.0, 1.0],
                "rotation": [0.0, 0.0, 0.0, 1.0]
            }
        }
    ],
    "lights": [
        {
            "type": "directional",
            "direction": [0.5, -1.0, -0.3],
            "color": [1.0, 0.95, 0.8],
            "intensity": 1.5
        }
    ]
}
```

- [ ] **Step 4: Commit**

```bash
git add engine.json assets/materials/sword.tmat assets/scenes/demo.tscene
git commit -m "feat: add engine config, material, and scene JSON for Part 6 example"
```

---

### Task 2: 创建示例应用源码

**Files:**
- Create: `src/Examples/Tumbler/main.cpp`
- Create: `src/Examples/Tumbler/CMakeLists.txt`

**Interfaces:**
- Consumes: `TumblerCore` (Engine, EngineConfig, SceneLoader, AssetDatabase, FScene)
- Produces: `App-Tumbler` 可执行目标

- [ ] **Step 1: 创建 `src/Examples/Tumbler/main.cpp`**

```cpp
#include "Core/Engine/Engine.h"
#include "Core/Engine/EngineConfig.h"
#include "Core/Scene/SceneLoader.h"
#include "Core/Assets/AssetDatabase.h"
#include "Core/GameSystem/FScene.h"
#include "Core/Utils/Log.h"

#include <iostream>

using namespace Tumbler;

int main() {
    LogInit();

    // ---- 1. 加载引擎配置 ----
    EngineConfig config;
    if (!config.LoadFromFile("engine.json")) {
        LOG_CRITICAL("Failed to load engine.json");
        return 1;
    }

    // ---- 2. 初始化引擎 ----
    Engine engine;
    if (!engine.Init(config)) {
        LOG_CRITICAL("Engine initialization failed");
        return 1;
    }

    // ---- 3. 加载场景 ----
    FScene scene;
    SceneLoader::Result result;
    SceneLoader loader;

    if (!loader.LoadFromFile(scene, "assets/scenes/demo.tscene",
                             *engine.GetAssetDatabase(), result)) {
        LOG_ERROR("Failed to load scene");
    } else {
        LOG_INFO("Scene loaded: {} mesh actors, {} light actors",
                  result.MeshActors.size(), result.LightActors.size());

        // 输出每个 Mesh Actor 的信息
        for (const auto* actor : result.MeshActors) {
            auto* mesh = actor->GetComponent<CStaticMesh>();
            if (mesh) {
                LOG_INFO("  Mesh: '{}' source='{}' cooked='{}'",
                          actor->Name, mesh->MeshSourcePath, mesh->CookedMeshPath);
            }
        }
    }

    // ---- 4. 主循环 ----
    LOG_INFO("Entering main loop...");
    engine.Run();

    // ---- 5. 关闭 ----
    engine.Shutdown();
    LogShutdown();
    return 0;
}
```

- [ ] **Step 2: 创建 `src/Examples/Tumbler/CMakeLists.txt`**

```cmake
add_executable(App-Tumbler main.cpp)

target_link_libraries(App-Tumbler PRIVATE TumblerCore)
target_compile_features(App-Tumbler PRIVATE cxx_std_20)

# Windows: 运行时 DLL 复制
if(WIN32)
    add_custom_command(TARGET App-Tumbler POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:App-Tumbler>
            $<TARGET_FILE_DIR:App-Tumbler>
        COMMAND_EXPAND_LISTS
    )
endif()

# 复制 assets 目录到可执行文件目录
add_custom_command(TARGET App-Tumbler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:App-Tumbler>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/engine.json"
        "$<TARGET_FILE_DIR:App-Tumbler>/engine.json"
)
```

- [ ] **Step 3: 修改 `src/Examples/CMakeLists.txt`** — 移除 TinyRendererModels

旧内容:
```cmake
add_subdirectory(Tumbler)
add_subdirectory(TinyRendererModels)
```

新内容:
```cmake
add_subdirectory(Tumbler)
```

- [ ] **Step 4: 修改根 `CMakeLists.txt`** — 取消注释 Examples

找到:
```cmake
# add_subdirectory(src/Examples)
```

替换为:
```cmake
add_subdirectory(src/Examples)
```

- [ ] **Step 5: Commit**

```bash
git add src/Examples/Tumbler/main.cpp src/Examples/Tumbler/CMakeLists.txt src/Examples/CMakeLists.txt CMakeLists.txt
git commit -m "feat: add App-Tumbler example with Engine API (Part 6)"
```

---

### Task 3: 构建 TumblerImporter 并生成 cooked 资产

- [ ] **Step 1: 配置并编译 TumblerImporter**

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DBUILD_TESTING=ON

cmake --build build --config Debug --target TumblerImporter --parallel
```

验证: `build\src\Tools\AssetImporter\Debug\TumblerImporter.exe --help` 输出帮助信息。

- [ ] **Step 2: 导入资产**

```powershell
.\build\src\Tools\AssetImporter\Debug\TumblerImporter.exe --input assets/scenes/demo.tscene --output cooked/
```

验证: 检查生成的文件:
- `cooked/meshes/Sting-Sword-lowpoly.tmesh`
- `cooked/textures/1.ttex`
- `cooked/materials/sword.tmat`
- `cooked/asset_map.json`

---

### Task 4: 构建并验证 App-Tumbler

- [ ] **Step 1: 编译 App-Tumbler**

```powershell
cmake --build build --config Debug --target App-Tumbler --parallel
```

验证: `build\src\Examples\Tumbler\Debug\App-Tumbler.exe` 存在。

- [ ] **Step 2: 复制 cooked 资产到可执行目录**

```powershell
xcopy /E /I /Y cooked build\src\Examples\Tumbler\Debug\cooked
```

- [ ] **Step 3: 运行 App-Tumbler**

```powershell
.\build\src\Examples\Tumbler\Debug\App-Tumbler.exe
```

预期终端输出:
```
[Log] Initialized
[EngineConfig] Loaded: 1280x720 'Tumbler'
[AssetDatabase] Loaded asset_map: 1 meshes, 1 textures, 1 materials
[AppWindow] Window created: 1280x720 'Tumbler'
[Engine] Initialized successfully.
[SceneLoader] Loaded scene 'Demo': 1 meshes, 1 lights
[Engine] Frame 60, running...
[Engine] Frame 120, running...
...
[Engine] Shutdown complete.
```

窗口出现（黑窗，无渲染内容），关闭窗口后正常退出。

- [ ] **Step 4: 运行全部测试确保无回归**

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

验证: 92/92 通过（或全部通过）。

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "chore: add cooked assets ignore, verify App-Tumbler builds and runs"
```
