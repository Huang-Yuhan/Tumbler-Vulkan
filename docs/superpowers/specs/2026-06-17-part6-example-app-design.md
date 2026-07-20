# Part 6: 示例应用骨架 — 设计规格

日期: 2026-06-17 | 分支: `nanite-integration`

## 目标

用新 Engine API 重写 Tumbler 示例应用，跑通 Engine 初始化 → 资产加载 → 主循环的完整链路。

## 范围

最小可用：初始化所有子系统，加载一个演示场景，进入主循环。目前没有 Renderer，屏幕为黑窗，但日志确认所有子系统正常启动。

## 文件变更

| 文件 | 操作 | 说明 |
|------|------|------|
| `engine.json` | 新建 | 窗口 (1280×720) + render 配置 |
| `assets/scenes/demo.tscene` | 新建 | 场景：Sword 模型 + 方向光 + 相机 |
| `assets/materials/sword.tmat` | 新建 | PBR 材质，引用 1.jpg 作为 albedo |
| `src/Examples/Tumbler/main.cpp` | 新建 | 入口 |
| `src/Examples/Tumbler/CMakeLists.txt` | 新建 | 构建目标 `App-Tumbler` |
| `src/Examples/CMakeLists.txt` | 修改 | 移除 TinyRendererModels 子目录 |
| `CMakeLists.txt` (根) | 修改 | 取消注释 `add_subdirectory(src/Examples)` |

## 资产管线

```
源文件:                                TumblerImporter →
  assets/models/Sting-Sword-lowpoly.obj    cooked/meshes/Sting-Sword-lowpoly.tmesh
  assets/textures/1.jpg                    cooked/textures/1.ttex
  assets/materials/sword.tmat              cooked/materials/sword.tmat
                                           cooked/asset_map.json
```

运行前需先执行 Importer 生成 `cooked/` 目录。

## engine.json

```json
{
  "window": { "width": 1280, "height": 720, "title": "Tumbler" },
  "render": { "cookedPath": "cooked/", "assetMap": "cooked/asset_map.json" }
}
```

## 场景 JSON (`assets/scenes/demo.tscene`)

```json
{
  "name": "Demo",
  "camera": {
    "fov": 60, "nearPlane": 0.1, "farPlane": 1000,
    "position": [0, 2, 8],
    "lookAt": [0, 1, 0]
  },
  "objects": [
    {
      "name": "Sword",
      "mesh": "assets/models/Sting-Sword-lowpoly.obj",
      "materials": ["assets/materials/sword.tmat"],
      "transform": {
        "position": [0, 1.5, 0],
        "scale": [1, 1, 1],
        "rotation": [0, 0, 0, 1]
      }
    }
  ],
  "lights": [
    {
      "type": "directional",
      "direction": [0.5, -1, -0.3],
      "color": [1, 0.95, 0.8],
      "intensity": 1.5
    }
  ]
}
```

## 材质 JSON (`assets/materials/sword.tmat`)

```json
{
  "name": "Sword",
  "albedo": "assets/textures/1.jpg",
  "normal": "",
  "metallicRoughness": "",
  "baseColorTint": [1, 1, 1],
  "roughness": 0.5,
  "metallic": 0.1
}
```

## main.cpp 关键逻辑

```cpp
int main() {
    EngineConfig config;
    config.LoadFromFile("engine.json");

    Engine engine;
    engine.Init(config);

    FScene scene;
    SceneLoader::Result result;
    SceneLoader loader;
    loader.LoadFromFile(scene, "assets/scenes/demo.tscene",
                        *engine.GetAssetDatabase(), result);

    // 日志：输出加载结果
    //  camera actor, mesh actors count, light actors count

    engine.Run();  // 主循环：每 60 帧输出 "Frame N, running..."

    engine.Shutdown();
}
```

## CMakeLists.txt（示例）

```cmake
add_executable(App-Tumbler main.cpp)
target_link_libraries(App-Tumbler PRIVATE TumblerCore)
target_compile_features(App-Tumbler PRIVATE cxx_std_20)
```

## 测试

- **编译**: `cmake --build build --target App-Tumbler`
- **运行**: 启动后窗口出现，终端输出 `[Engine] Initialized successfully.` + `[SceneLoader] Loaded scene 'Demo'...` + 每60帧状态日志
- **退出**: 关闭窗口 → `[Engine] Shutdown complete.`
- **CI**: 现有 92 个测试不受影响

## 不做

- 不实现任何渲染（Renderer 是 Phase 4+ 的事）
- 不添加 ImGui / 编辑器 / 控制台
- 不添加 FPS 相机控制（InputManager 存在但暂不接入）
- 不复活 TinyRendererModels 示例
