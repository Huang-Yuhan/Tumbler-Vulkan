# 编辑器与调试工具

Tumbler 集成 ImGui 作为内置编辑器，Core 层提供 `RuntimeConsole` 运行时命令控制台。

## 1. 编辑器面板架构

编辑器 UI 由 `AppLogic::DrawEditorUI()` 组织，分为独立窗口和统一调试窗口：

```
DrawEditorUI()
├── ValidateSelectedActor()
├── DrawDebugPanel()          // 统一调试窗口（CollapsingHeader 布局）
│   ├── Performance section   // DrawPerformanceSection()
│   ├── Camera section        // DrawCameraSection()
│   ├── Lighting section      // DrawLightingSection()
│   ├── Rendering section     // DrawRenderingSection()
│   └── G-Buffer section      // Deferred 模式下显示 Albedo/Normal 预览
├── DrawSceneHierarchyPanel() // 独立窗口
└── DrawInspectorPanel()      // 独立窗口
```

调试窗口默认打开，可通过 `EditorSessionState::ShowDebugPanel` 控制。

### 1.1 性能统计（Performance Section）

实时数据：FPS、Frame Time (ms)、Draw Call 计数、Point Light 数量、当前 Render Path。

帧时间历史图表（100 帧滚动），Y 轴上限 33.3ms（≈30 FPS 底线）。

**代码**：`AppLogic.cpp` — `DrawPerformanceSection()`

### 1.2 光源设置（Lighting Section）

- 主光源（MainLight）：位置 DragFloat3、颜色 ColorEdit3、强度 SliderFloat (0-200)
- 如 MainLight 不存在则显示提示

**代码**：`AppLogic.cpp` — `DrawLightingSection()`

### 1.3 相机设置（Camera Section）

- 位置 DragFloat3 + 旋转欧拉角 DragFloat3
- FOV (30-120)、Move Speed (1-50)、Mouse Sensitivity (0.1-5)

**代码**：`AppLogic.cpp` — `DrawCameraSection()`

### 1.4 渲染设置（Rendering Section）

显示当前 Render Path + Combo 切换（Forward / Deferred / GPU Driven(WIP)）。切换 GPU Driven 时输出警告日志。

**代码**：`AppLogic.cpp` — `DrawRenderingSection()`

### 1.5 G-Buffer 预览（G-Buffer Section）

仅在 Deferred 模式下显示。通过 `DebugTexturePreview`（Core 层）管理 Vulkan 资源：

```cpp
DebugPreview.SetImage(0, Renderer->GetGBufferAlbedoImageView());
DebugPreview.SetImage(1, Renderer->GetGBufferNormalImageView());
```

预览窗口左右并列显示 Albedo 和 Normal，保持交换链宽高比。

**代码**：`AppLogic.cpp` — `DrawDebugPanel()` 中的 G-Buffer CollapsingHeader

### 1.6 场景层级（Scene Hierarchy）

独立窗口，树形显示所有 Actor（根节点平铺，子节点根据 `Transform` 父子关系折叠）。点击选中，`SessionState->SelectedActor` 同步更新。

**代码**：`AppLogic.cpp` — `DrawSceneHierarchyPanel()`

### 1.7 Inspector

独立窗口，显示 `SelectedActor` 的 Name + Transform + 各 Component 的 `OnDrawUI()`。通过 `ValidateSelectedActor()` 检测选中 Actor 是否已被销毁。

**代码**：`AppLogic.cpp` — `DrawInspectorPanel()`

## 2. 主循环时序

```cpp
while (!window.ShouldClose()) {
    window.PollEvents();

    // 窗口 resize 检测 + Swapchain 重建
    if (window.IsFramebufferResized()) { ... }

    // 输入：先注入 UI 焦点状态，再 Tick
    inputManager.SetUIFocused(ImGui::GetIO().WantCaptureMouse
                            || ImGui::GetIO().WantCaptureKeyboard);
    inputManager.Tick();
    ui_manager.TickInput();          // 驱动 RuntimeConsole

    logic.Tick(frameTime);

    // 数据提取（在 UI 之前）
    scene->ExtractRenderPackets(renderPackets);

    // UI
    ui_manager.BeginFrame();
    logic.UpdatePerformanceStats(frameTime, renderPackets.size());
    logic.DrawEditorUI();
    ui_manager.EndFrame();

    // 渲染
    SceneViewData viewData = scene->GenerateSceneView(...);
    viewData.RenderPath = renderSettings.CurrentRenderPath;
    renderer.Render(viewData, renderPackets, [&](VkCommandBuffer cmd, uint32_t idx) {
        ui_manager.RecordDrawCommands(cmd, &renderer, idx);
    });
}
```

关键点：
- `SetUIFocused()` 在 `Tick()` 之前注入，避免 InputManager 直接依赖 ImGui
- `ExtractRenderPackets()` 在 `BeginFrame()` 之前，UI 可观察提取结果
- `GenerateSceneView()` 在 `EndFrame()` 之后，可获取 UI 中修改的渲染参数

## 3. RuntimeConsole 运行时控制台

### 3.1 基本操作

| 按键 | 功能 |
|------|------|
| `~`（可配置） | 打开/关闭 |
| `Enter` | 执行命令 |
| `Up/Down` | 浏览历史 |
| `Tab` | 自动补全命令名和参数 |

控制台打开时自动阻断游戏输入（`SetGameplayInputBlocked(true)`），关闭时恢复。

### 3.2 切换键配置

```cpp
ui_manager.GetConsole().SetToggleKey(EKeyCode::F1);  // 改为 F1
```

默认键为 `EKeyCode::GraveAccent`。

### 3.3 注册自定义命令

```cpp
console.RegisterCommand({
    .Name = "mycmd",
    .Usage = "mycmd <arg>",
    .Description = "Does something.",
    .Handler = [&console](const std::vector<std::string>& args) {
        console.AddMessage(EConsoleMessageType::Info, "Executed!");
    }
});
```

完整命令参考见 [控制台命令参考](console.md)。

## 4. 编辑状态管理

### 4.1 EditorSessionState（选中状态）

```cpp
struct EditorSessionState {
    FActor* SelectedActor = nullptr;   // Hierarchy 面板设置，Inspector / 控制台消费
    bool ShowDebugPanel = true;        // 控制调试窗口显示
};
```

### 4.2 RenderSettings（渲染配置）

```cpp
struct RenderSettings {
    ERenderPath CurrentRenderPath = ERenderPath::Forward;  // 控制台 / Combo 修改，main.cpp 消费
};
```

`CurrentRenderPath` 从 `EditorSessionState` 拆分到 `RenderSettings`，职责分离。

## 5. 材质参数编辑

通过 Inspector 面板中 `CMeshRenderer::OnDrawUI()` 编辑材质参数：

```cpp
// 编辑单个参数（持久化到 UBO）
material->SetFloat("Roughness", 0.5f);
material->UpdateUBO();  // 直接写入持久映射缓冲，不重建描述符
```

**注意**：必须使用 `UpdateUBO()` 而非 `ApplyChanges()`。`ApplyChanges()` 仅更新描述符写入结构，不写入 UBO 缓冲，参数值会丢失。

## 6. 条件显示调试信息

```cpp
#ifndef NDEBUG
if (ImGui::Begin("Debug Info")) {
    // 仅在 Debug 模式下编译和显示
    ImGui::Text("...");
    ImGui::End();
}
#endif
```

使用 `#ifndef NDEBUG`（Debug 模式未定义 `NDEBUG`），而非 `#ifdef NDEBUG`（Release 模式）。

## 7. 常见模式

### 添加新面板

1. 在 `AppLogic` 中添加 `DrawXxxSection()` 私有方法
2. 在 `DrawDebugPanel()` 中通过 `ImGui::CollapsingHeader("Section Name")` 挂载
3. 如需独立窗口，在 `DrawEditorUI()` 中调用

### 用控制台调试

```
render.path deferred    # 切换到 Deferred 查看 G-Buffer 预览
actors                  # 列出所有 Actor
select MainLight        # 选中主光源
actor.move selected 0 2 0  # 上移选中的 Actor
```
