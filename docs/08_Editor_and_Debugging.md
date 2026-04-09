# 编辑器与调试工具 (Editor & Debugging Tools)

Tumbler 引擎集成了 ImGui 作为内置的调试和编辑器工具，并在 Core 层提供了运行时命令控制台（`RuntimeConsole`），用于做接近 Unreal Console 的即时调试。

## 1. 内置编辑器面板

Tumbler 引擎内置了多个实用的编辑器面板，位于 `AppLogic` 类中：

### 1.1 性能统计面板 (Performance Panel)

显示实时性能数据，帮助你监控和优化渲染性能。

**功能包括：**
- FPS（每秒帧数）
- 帧时间（毫秒）
- Draw Call 计数
- 帧时间历史图表（100帧滚动记录）

**代码位置：** `src/Examples/Tumbler/AppLogic.cpp` - `DrawPerformancePanel()`

### 1.2 光源设置面板 (Light Settings)

实时调整场景光源参数。

**功能包括：**
- 光源位置拖拽调整
- 光源颜色编辑 (ColorEdit3)
- 光源强度滑块 (0-200)

**代码位置：** `src/Examples/Tumbler/AppLogic.cpp` - `DrawLightPanel()`

### 1.3 相机参数面板 (Camera Panel)

调整相机参数和漫游设置。

**功能包括：**
- 相机位置实时调整
- 相机旋转（欧拉角）编辑
- FOV 视场角调整（30-120度）
- 移动速度调整（1-50）
- 鼠标灵敏度调整（0.1-5）

**代码位置：** `src/Examples/Tumbler/AppLogic.cpp` - `DrawCameraPanel()`

### 1.4 场景层级面板 (Scene Hierarchy)

显示和选择场景中的所有 Actor。

**功能包括：**
- 列表显示所有 Actor
- 点击选择 Actor
- 高亮当前选中项

**代码位置：** `src/Examples/Tumbler/AppLogic.cpp` - `DrawSceneHierarchyPanel()`

### 1.5 材质编辑器 (Material Editor)

完整的 PBR 材质参数编辑器。

**功能包括：**
- Base Color（基础颜色）- RGBA 颜色选择器
- Roughness（粗糙度）- 0.0 到 1.0 滑块
- Metallic（金属度）- 0.0 到 1.0 滑块
- Normal Strength（法线强度）- 0.0 到 2.0 滑块
- Two Sided（双面渲染）- 复选框

**代码位置：** `src/Examples/Tumbler/AppLogic.cpp` - `DrawMaterialPanel()`

### 1.6 运行时控制台 (Runtime Console)

运行时控制台由 `UIManager` 托管，底层实现位于：

- `src/Core/Editor/RuntimeConsole.h`
- `src/Core/Editor/RuntimeConsole.cpp`

Tumbler 示例命令绑定位于：

- `src/Examples/Tumbler/TumblerConsoleBindings.cpp`

**交互方式：**
- `~`：打开/关闭控制台
- `Enter`：执行命令
- `Up/Down`：浏览历史
- `Tab`：自动补全命令名和已支持的参数

**内建命令：**
- `help`
- `clear`
- `history`

**Tumbler 示例命令：**
- `actors`
- `select <ActorName|none>`
- `actor.move <ActorName|selected> <x> <y> <z>`
- `actor.rotate <ActorName|selected> <pitch> <yaw> <roll>`
- `actor.scale <ActorName|selected> <x> <y> <z>`
- `camera.pos <x> <y> <z>`
- `camera.speed <value>`
- `light.intensity <ActorName> <value>`
- `light.color <ActorName> <r> <g> <b>`
- `render.path <forward|deferred|gpu>`
- `spawn.light <Name> <x> <y> <z> <intensity>`
- `destroy <ActorName|selected>`

### 1.7 控制台实现细节

`RuntimeConsole` 不是简单的 ImGui 输入框包装，而是一个小型命令运行时，内部主要由以下几部分构成：

#### 命令存储

- `std::vector<ConsoleCommandDefinition> Commands`
- `std::unordered_map<std::string, size_t> CommandLookup`

命令注册时会把 `Name` 归一化为小写后写入 `CommandLookup`，因此命令关键字大小写不敏感。

#### 输入与日志缓存

- `InputBuffer`：固定大小的字符缓冲区
- `Messages`：日志列表，带消息类型颜色
- `History`：历史命令
- `HistoryIndex`：历史浏览游标

日志区会在 `AddMessage()` 后自动把 `bScrollToBottom` 置位，在下一次绘制时滚到最底部。

#### 命令解析

执行输入时，控制台会：

1. 先 `Trim` 输入
2. 记录原始命令到日志区，前缀为 `>`
3. 用 `TokenizeCommand()` 做参数分词
4. 支持双引号包裹参数
5. 取第一个 token 作为命令名，其余 token 作为参数列表

也就是说：

```text
spawn.light "Point Light A" 0 2 0 20
```

会被解析为：

- 命令名：`spawn.light`
- 参数：`Point Light A`, `0`, `2`, `0`, `20`

#### 输入回调

控制台通过 `ImGui::InputText()` 的两个 callback flag 实现高级交互：

- `ImGuiInputTextFlags_CallbackHistory`
- `ImGuiInputTextFlags_CallbackCompletion`

对应到：

- `OnInputTextHistory()`
- `OnInputTextCompletion()`

### 1.8 `Tab` 自动补全实现

当前补全逻辑分两层：

#### 命令名补全

当光标位于第一个 token 内时：

1. 遍历 `Commands`
2. 用大小写不敏感前缀匹配筛出候选
3. 如果只有一个候选，直接补全并自动追加空格
4. 如果有多个候选，补到最长公共前缀
5. 如果最长公共前缀不足以继续缩小范围，则在日志区打印候选列表

#### 参数补全

`ConsoleCommandDefinition` 支持一个可选的：

```cpp
using ConsoleAutocompleteHandler =
    std::function<std::vector<std::string>(
        const std::vector<std::string>& args,
        size_t activeArgIndex)>;
```

控制台会把：
- 当前命令已输入的参数
- 当前正在补全的是第几个参数

传给命令自己的补全函数，然后把返回的候选统一做：

- 前缀过滤
- 排序
- 最长公共前缀计算
- 必要时自动加引号

这样 Core 只负责“补全机制”，而不需要知道 Tumbler 场景里有哪些 Actor。

### 1.9 Tumbler 命令绑定实现

Tumbler 的命令实现位于 `TumblerConsoleBindings.cpp`，这个文件负责两件事：

1. 注册命令处理函数
2. 注册命令自己的参数补全函数

例如：

- `select` / `destroy` / `actor.*`
  会从 `FScene::GetAllActors()` 收集 Actor 名称
- `light.intensity` / `light.color`
  只返回带 `CPointLight` 组件的 Actor
- `render.path`
  返回固定枚举：`forward`、`deferred`、`gpu`

这样可以保证：
- Core 不依赖 Tumbler 场景结构
- 场景补全候选始终来自运行时真实状态
- 未来别的 Example 可以注册自己的命令和补全逻辑

## 2. 编辑器架构

现在的编辑器状态分为三层：

1. `UIManager`
   - 管理 ImGui 生命周期
   - 托管 `RuntimeConsole`
2. `EditorSessionState`
   - 保存共享编辑状态，如 `SelectedActor` 和 `CurrentRenderPath`
3. `AppLogic`
   - 负责场景 Hierarchy、Inspector、性能面板等业务 UI

简化结构如下：

```cpp
UIManager
├── ImGui backend
└── RuntimeConsole

EditorSessionState
├── SelectedActor
└── CurrentRenderPath

AppLogic
├── Scene Hierarchy
├── Inspector / Material UI
└── Performance / Camera / Light panels
```

### 2.1 主循环中的时序

在 `App-Tumbler` 中，控制台和编辑器的时序是：

1. `window.PollEvents()`
2. `inputManager.Tick()`
3. `ui_manager.TickInput()`
4. `logic.Tick(frameTime)`
5. `ui_manager.BeginFrame()`
6. `logic.DrawEditorUI()`
7. `ui_manager.EndFrame()`
8. `renderer.Render(...)`

关键点是第 3 步：控制台会在逻辑更新前就决定本帧是否阻断 Gameplay 输入，因此相机不会在控制台刚打开的那一帧继续吃到 WASD 或鼠标位移。

## 3. ImGui 简介

[ImGui](https://github.com/ocornut/imgui) 是一个用于 C++ 的无依赖、单头文件、即时模式的图形用户界面库。它特别适合用于：
- 调试工具
- 编辑器控制面板
- 参数调节面板
- 性能分析器

## 4. UIManager 类

Tumbler 引擎提供 `UIManager` 类来封装 ImGui 的初始化、帧管理和 Vulkan 集成。

### 4.1 初始化

```cpp
UIManager ui_manager;
ui_manager.Init(&window, &renderer, &inputManager);
```

### 4.2 帧生命周期

```cpp
while (!window.ShouldClose()) {
    window.PollEvents();
    inputManager.Tick();
    ui_manager.TickInput();
    logic.Tick(frameTime);
    
    // --- UI 绘制开始 ---
    ui_manager.BeginFrame();
    
    logic.DrawEditorUI();
    
    ui_manager.EndFrame();
    // --- UI 绘制结束 ---
    
    // ... 渲染 ...
}
```

### 4.3 清理

```cpp
vkDeviceWaitIdle(renderer.GetDevice());
ui_manager.Cleanup(renderer.GetDevice());
```

## 5. 渲染集成

UI 绘制需要在 Vulkan 渲染循环中作为回调传入：

```cpp
renderer.Render(
    viewData, 
    renderPackets, 
    [&](VkCommandBuffer cmd, uint32_t imageIndex) {
        ui_manager.RecordDrawCommands(cmd, &renderer, imageIndex);
    }
);
```

`ui_manager.EndFrame()` 会在 `ImGui::Render()` 之前统一绘制运行时控制台窗口。

## 6. 常用 ImGui 控件

### 6.1 窗口

```cpp
// 创建窗口
ImGui::Begin("My Window");

// 窗口内容...

ImGui::End();
```

### 6.2 文本

```cpp
ImGui::Text("Hello, World!");
ImGui::Text("FPS: %.1f", fps);
ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error!");
```

### 6.3 按钮

```cpp
if (ImGui::Button("Click Me!")) {
    LOG_INFO("Button clicked!");
}

// 带大小的按钮
if (ImGui::Button("Big Button", ImVec2(100, 50))) {
    // ...
}
```

### 6.4 滑块

```cpp
// 浮点数滑块
static float floatValue = 0.5f;
ImGui::SliderFloat("Float Slider", &floatValue, 0.0f, 1.0f);

// 整数滑块
static int intValue = 50;
ImGui::SliderInt("Int Slider", &intValue, 0, 100);

// 角度滑块（自动转换弧度/度）
static float angle = 0.0f;
ImGui::SliderAngle("Angle", &angle);
```

### 6.5 拖拽控件

```cpp
// 单个浮点数
static float x = 0.0f;
ImGui::DragFloat("X", &x, 0.1f, -10.0f, 10.0f);

// 向量
static glm::vec3 position(0, 0, 0);
ImGui::DragFloat3("Position", &position.x, 0.1f);

// 颜色
static glm::vec3 color(1, 1, 1);
ImGui::ColorEdit3("Color", &color.x);
```

### 6.6 复选框

```cpp
static bool enabled = true;
ImGui::Checkbox("Enabled", &enabled);
```

### 6.7 下拉菜单

```cpp
static int currentItem = 0;
const char* items[] = { "Item 1", "Item 2", "Item 3" };
ImGui::Combo("Combo", &currentItem, items, IM_ARRAYSIZE(items));
```

### 6.8 树节点

```cpp
if (ImGui::TreeNode("Advanced Options")) {
    ImGui::Text("Option 1");
    ImGui::Text("Option 2");
    ImGui::TreePop();
}
```

## 7. 实际示例：PBR 调试面板

以下是项目中实际使用的 ImGui 调试面板示例：

```cpp
ImGui::Begin("PBR Debug Engine");

// 查找并控制光源
if (FActor* mainLight = logic.GetScene()->FindActorByName("MainLight")) {
    if (auto* pl = mainLight->GetComponent<CPointLight>()) {
        // 控制光源位置
        glm::vec3 pos = mainLight->Transform.GetPosition();
        if (ImGui::DragFloat3("Light Pos", &pos.x, 0.1f, -10.0f, 10.0f)) {
            mainLight->Transform.SetPosition(pos);
        }
        
        // 控制光源颜色
        ImGui::ColorEdit3("Light Color", &pl->Color.x);
        
        // 控制光源强度
        ImGui::SliderFloat("Light Power", &pl->Intensity, 0.0f, 200.0f);
    }
} else {
    ImGui::Text("MainLight not found");
}

// 显示 FPS
ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

ImGui::End();
```

## 8. 材质参数调试

可以创建一个通用的材质调试面板：

```cpp
void ShowMaterialDebugPanel(FMaterialInstance* matInstance) {
    ImGui::Begin("Material Editor");
    
    // 基础颜色
    static glm::vec4 baseColor(1, 1, 1, 1);
    if (ImGui::ColorEdit4("Base Color", &baseColor.x)) {
        matInstance->SetVector("BaseColorTint", baseColor);
        matInstance->UpdateUBO(); // 快速更新 UBO，不重新绑定描述符
    }
    
    // 粗糙度
    static float roughness = 0.5f;
    if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f)) {
        matInstance->SetFloat("Roughness", roughness);
        matInstance->UpdateUBO();
    }
    
    // 金属度
    static float metallic = 0.0f;
    if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f)) {
        matInstance->SetFloat("Metallic", metallic);
        matInstance->UpdateUBO();
    }
    
    ImGui::End();
}
```

### 重要提示：UpdateUBO() vs ApplyChanges()

- **`UpdateUBO()`** - 仅更新 UBO 数据到持久映射的内存，不重新绑定描述符
  - 适用于编辑器中频繁的参数调整
  - 避免 Vulkan 验证错误
  - 性能更好

- **`ApplyChanges()`** - 完整的材质更新，包括重新绑定描述符
  - 适用于初始化或切换纹理时
  - 会调用 `vkUpdateDescriptorSets()`

## 9. 性能分析

### 9.1 内置 FPS 显示

```cpp
ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
```

### 9.2 使用 ImGui::PlotLines 绘制性能图表

```cpp
// 保存帧率历史
static float fpsHistory[100] = {0};
static int fpsHistoryIndex = 0;

// 每帧更新
fpsHistory[fpsHistoryIndex] = ImGui::GetIO().Framerate;
fpsHistoryIndex = (fpsHistoryIndex + 1) % 100;

// 绘制图表
ImGui::PlotLines("FPS", fpsHistory, 100, 0, nullptr, 0.0f, 120.0f, ImVec2(0, 80));
```

## 10. ImGui 样式定制

可以自定义 ImGui 的外观：

```cpp
ImGuiStyle& style = ImGui::GetStyle();

// 圆角
style.WindowRounding = 8.0f;
style.FrameRounding = 4.0f;

// 颜色
ImVec4* colors = style.Colors;
colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.95f);
colors[ImGuiCol_Button] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
colors[ImGuiCol_ButtonHovered] = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
colors[ImGuiCol_ButtonActive] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
```

## 11. 调试技巧

### 11.1 条件显示

```cpp
// 只在 Debug 模式显示
#ifdef NDEBUG
if (ImGui::Begin("Debug Info")) {
    // ...
}
ImGui::End();
#endif

// 按键切换显示
static bool showDebug = true;
if (inputManager.IsActionJustPressed("ToggleDebug")) {
    showDebug = !showDebug;
}
if (showDebug) {
    // ...
}
```

### 11.2 日志窗口

```cpp
// 创建日志窗口
ImGui::Begin("Log");

// 假设你有一个日志消息队列
for (const auto& msg : logMessages) {
    ImGui::TextUnformatted(msg.c_str());
}

// 自动滚动
if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
    ImGui::SetScrollHereY(1.0f);
}

ImGui::End();
```

### 11.3 场景层级视图

```cpp
void ShowSceneHierarchy(FScene* scene) {
    ImGui::Begin("Scene Hierarchy");
    
    for (const auto& actor : scene->GetAllActors()) {
        if (ImGui::TreeNode(actor->Name.c_str())) {
            // 显示组件
            for (const auto& comp : actor->Components) {
                ImGui::Text("- %s", typeid(*comp).name());
            }
            ImGui::TreePop();
        }
    }
    
    ImGui::End();
}
```

## 12. 常见问题

### Q: ImGui 窗口不显示？
A: 确保在 `BeginFrame()` 和 `EndFrame()` 之间调用 ImGui 绘制函数，并且在 `Render()` 回调中调用了 `RecordDrawCommands()`。

### Q: 游戏输入被 ImGui 拦截？
A: 使用 `inputManager.IsUIFocused()` 检查 UI 是否获得焦点，只有在没有焦点时才处理游戏输入。

### Q: 如何保存 ImGui 的布局？
A: ImGui 会自动保存布局到 `imgui.ini` 文件（在项目根目录）。

### Q: 材质编辑器导致 Vulkan 验证错误？
A: 使用 `UpdateUBO()` 而不是 `ApplyChanges()` 来更新材质参数，避免在描述符集被 GPU 使用时重新绑定。
