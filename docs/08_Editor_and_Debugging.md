# 编辑器与调试工具 (Editor & Debugging Tools)

Tumbler 引擎集成了 ImGui 作为内置的调试和编辑器工具，提供了强大的即时可视化调试能力。

## 1. ImGui 简介

[ImGui](https://github.com/ocornut/imgui) 是一个用于 C++ 的无依赖、单头文件、即时模式的图形用户界面库。它特别适合用于：
- 调试工具
- 编辑器控制面板
- 参数调节面板
- 性能分析器

## 2. UIManager 类

Tumbler 引擎提供 `UIManager` 类来封装 ImGui 的初始化、帧管理和 Vulkan 集成。

### 2.1 初始化

```cpp
UIManager ui_manager;
ui_manager.Init(&window, &renderer);
```

### 2.2 帧生命周期

```cpp
while (!window.ShouldClose()) {
    // ... 游戏逻辑 ...
    
    // --- UI 绘制开始 ---
    ui_manager.BeginFrame();
    
    // 在这里绘制 ImGui 控件
    ImGui::Begin("Debug Panel");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::End();
    
    ui_manager.EndFrame();
    // --- UI 绘制结束 ---
    
    // ... 渲染 ...
}
```

### 2.3 清理

```cpp
vkDeviceWaitIdle(renderer.GetDevice());
ui_manager.Cleanup(renderer.GetDevice());
```

## 3. 渲染集成

UI 绘制需要在 Vulkan 渲染循环中作为回调传入：

```cpp
renderer.Render(
    viewData, 
    renderPackets, 
    [&](VkCommandBuffer cmd) {
        // 在命令缓冲中录制 ImGui 绘制命令
        ui_manager.RecordDrawCommands(cmd);
    }
);
```

## 4. 常用 ImGui 控件

### 4.1 窗口

```cpp
// 创建窗口
ImGui::Begin("My Window");

// 窗口内容...

ImGui::End();
```

### 4.2 文本

```cpp
ImGui::Text("Hello, World!");
ImGui::Text("FPS: %.1f", fps);
ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error!");
```

### 4.3 按钮

```cpp
if (ImGui::Button("Click Me!")) {
    LOG_INFO("Button clicked!");
}

// 带大小的按钮
if (ImGui::Button("Big Button", ImVec2(100, 50))) {
    // ...
}
```

### 4.4 滑块

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

### 4.5 拖拽控件

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

### 4.6 复选框

```cpp
static bool enabled = true;
ImGui::Checkbox("Enabled", &enabled);
```

### 4.7 下拉菜单

```cpp
static int currentItem = 0;
const char* items[] = { "Item 1", "Item 2", "Item 3" };
ImGui::Combo("Combo", &currentItem, items, IM_ARRAYSIZE(items));
```

### 4.8 树节点

```cpp
if (ImGui::TreeNode("Advanced Options")) {
    ImGui::Text("Option 1");
    ImGui::Text("Option 2");
    ImGui::TreePop();
}
```

## 5. 实际示例：PBR 调试面板

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

## 6. 材质参数调试

可以创建一个通用的材质调试面板：

```cpp
void ShowMaterialDebugPanel(FMaterialInstance* matInstance) {
    ImGui::Begin("Material Editor");
    
    // 基础颜色
    static glm::vec4 baseColor(1, 1, 1, 1);
    if (ImGui::ColorEdit4("Base Color", &baseColor.x)) {
        matInstance->SetVector("BaseColorTint", baseColor);
        matInstance->ApplyChanges();
    }
    
    // 粗糙度
    static float roughness = 0.5f;
    if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f)) {
        matInstance->SetFloat("Roughness", roughness);
        matInstance->ApplyChanges();
    }
    
    // 金属度
    static float metallic = 0.0f;
    if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f)) {
        matInstance->SetFloat("Metallic", metallic);
        matInstance->ApplyChanges();
    }
    
    ImGui::End();
}
```

## 7. 性能分析

### 7.1 内置 FPS 显示

```cpp
ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
```

### 7.2 使用 ImGui::PlotLines 绘制性能图表

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

## 8. ImGui 样式定制

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

## 9. 调试技巧

### 9.1 条件显示

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

### 9.2 日志窗口

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

### 9.3 场景层级视图

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

## 10. 常见问题

### Q: ImGui 窗口不显示？
A: 确保在 `BeginFrame()` 和 `EndFrame()` 之间调用 ImGui 绘制函数，并且在 `Render()` 回调中调用了 `RecordDrawCommands()`。

### Q: 游戏输入被 ImGui 拦截？
A: 使用 `inputManager.IsUIFocused()` 检查 UI 是否获得焦点，只有在没有焦点时才处理游戏输入。

### Q: 如何保存 ImGui 的布局？
A: ImGui 会自动保存布局到 `imgui.ini` 文件（在项目根目录）。
