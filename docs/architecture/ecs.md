# 游戏系统架构 (Game System Architecture)

Tumbler 引擎采用轻量级的实体-组件系统（ECS 变体）来组织游戏世界。这种设计提供了极高的灵活性和可扩展性。

## 1. 核心概念

### 1.1 组合优于继承 (Composition Over Inheritance)

传统的游戏对象设计通常使用继承树（如 `GameObject` → `MovableObject` → `Character`），这种方式在复杂场景下会变得僵化且难以维护。

Tumbler 引擎采用**组合模式**：
- **FActor**: 游戏世界中的"容器"，本身不具备任何功能
- **Component**: 实现具体功能的"模块"，通过挂载到 Actor 上赋予其能力

这种设计使得对象功能可以任意组合，例如：
- 一个物体可以同时是光源、可移动的和有网格的
- 添加新功能只需创建新的 Component，无需修改现有代码

## 2. FActor (游戏实体)

`FActor` 是场景中的基本单位，它是一个"空壳"，只包含：
- 名称 (`Name`)
- 变换组件 (`Transform` - 位置、旋转、缩放)
- 组件列表 (`Components`)

### 2.1 创建 Actor

**重要**: 不要直接使用 `new FActor()`，始终通过 `FScene::CreateActor()` 工厂方法创建：

```cpp
// 在场景中创建一个 Actor
FActor* myActor = Scene->CreateActor("MyAwesomeActor");
```

### 2.2 挂载组件

使用模板方法 `AddComponent<T>()` 为 Actor 添加功能：

```cpp
// 添加网格渲染组件
CMeshRenderer* meshRenderer = myActor->AddComponent<CMeshRenderer>();

// 添加相机组件
CCamera* camera = myActor->AddComponent<CCamera>();

// 添加点光源组件
CPointLight* light = myActor->AddComponent<CPointLight>();
```

### 2.3 访问组件

```cpp
// 获取单个组件
CMeshRenderer* renderer = myActor->GetComponent<CMeshRenderer>();

// 获取多个同类型组件
std::vector<CMeshRenderer*> allRenderers = myActor->GetComponents<CMeshRenderer>();

// 特殊：获取 Transform（因为它是成员变量，不是 Component）
CTransform* transform = myActor->GetComponent<CTransform>();
```

## 3. Component (组件)

所有组件都继承自基类 `Component`，它提供：
- `Owner` 指针：指向挂载的 Actor
- `Update(float DeltaTime)` 虚函数：每帧更新

### 3.1 创建自定义组件

```cpp
class CMyCustomComponent : public Component {
public:
    void Update(float DeltaTime) override {
        // 每帧执行的逻辑
        LOG_INFO("Component updating! DeltaTime: {}", DeltaTime);
    }
    
    void MyCustomMethod() {
        // 自定义功能
        if (Owner) {
            Owner->Transform.Translate(glm::vec3(0, 0, 1) * DeltaTime);
        }
    }
};
```

### 3.2 内置组件列表

Tumbler 引擎目前提供以下内置组件：

| 组件 | 功能 |
|------|------|
| `CTransform` | 位置、旋转、缩放（作为 FActor 成员） |
| `CMeshRenderer` | 渲染 3D 网格 |
| `CCamera` | 摄像机视角 |
| `CFirstPersonCamera` | 第一人称漫游摄像机 |
| `CPointLight` | 点光源 |
| `CDirectionalLight` | 平行光 |

## 4. FScene (场景)

`FScene` 是所有 Actor 的容器，管理整个游戏世界的生命周期。

### 4.1 场景功能

```cpp
// 创建场景
std::unique_ptr<FScene> Scene = std::make_unique<FScene>();

// 每帧更新场景逻辑
Scene->Tick(deltaTime);

// 查找 Actor
FActor* found = Scene->FindActorByName("Player");

// 安全销毁 Actor（延迟到帧末尾）
Scene->DestroyActor(someActor);
```

### 4.2 渲染数据提取

`FScene` 的核心职责之一是为渲染器准备纯净的数据：

```cpp
// 1. 提取所有可渲染物体的 RenderPacket
std::vector<RenderPacket> packets;
Scene->ExtractRenderPackets(packets);

// 2. 生成特定相机视角的 SceneViewData
SceneViewData viewData = Scene->GenerateSceneView(
    camera, 
    &camera->GetOwner()->Transform, 
    aspectRatio
);

// 3. 将数据传给渲染器
renderer.Render(viewData, packets, uiCallback);
```

这种设计完美体现了**逻辑与渲染分离**的原则：
- 场景管理逻辑世界
- 渲染器只处理数据，不知道 Actor 的存在

## 5. 生命周期管理

### 5.1 Actor 销毁

**重要**: 不要直接 `delete` Actor，使用 `FScene::DestroyActor()`：

```cpp
// ❌ 错误做法
delete myActor;  // 可能导致崩溃，因为渲染器可能还在使用

// ✅ 正确做法
Scene->DestroyActor(myActor);  // 延迟到帧末尾安全销毁
```

内部实现：
```cpp
// FScene 维护一个待删除队列
std::vector<FActor*> PendingKillActors;

// 在 Tick() 末尾统一清理
void FScene::Tick(float deltaTime) {
    // ... 先更新所有 Actor ...
    
    // 最后清理
    for (FActor* actor : PendingKillActors) {
        // 安全删除
    }
    PendingKillActors.clear();
}
```

### 5.2 所有权模型

Tumbler 引擎使用明确的所有权规则：

```cpp
class FScene {
    // Scene 拥有所有 Actor（使用 unique_ptr）
    std::vector<std::unique_ptr<FActor>> Actors;
};

class FActor {
    // Actor 拥有所有 Component
    std::vector<std::unique_ptr<Component>> Components;
};

class Component {
    // Component 只持有 Owner 的裸指针（不拥有）
    FActor* Owner = nullptr;
};
```

这种设计确保：
- 资源不会泄漏
- 销毁顺序清晰明确
- 循环引用风险最低

## 6. 使用示例

### 6.1 创建一个完整的游戏对象

```cpp
// 1. 创建场景
auto Scene = std::make_unique<FScene>();

// 2. 创建主角 Actor
FActor* player = Scene->CreateActor("Player");

// 3. 添加第一人称相机
CFirstPersonCamera* fpsCam = player->AddComponent<CFirstPersonCamera>();

// 4. 创建光源
FActor* light = Scene->CreateActor("MainLight");
light->Transform.SetPosition(glm::vec3(0, 5, 0));

CPointLight* pointLight = light->AddComponent<CPointLight>();
pointLight->Color = glm::vec3(1, 0.9f, 0.8f);
pointLight->Intensity = 100.0f;

// 5. 创建物体
FActor* sword = Scene->CreateActor("Sword");
sword->Transform.SetPosition(glm::vec3(0, 0, -3));

CMeshRenderer* swordRenderer = sword->AddComponent<CMeshRenderer>();
swordRenderer->SetMesh(AssetMgr->GetOrLoadMesh("StingSword", "models/sword.obj"));
swordRenderer->SetMaterial(pbrMaterial->CreateInstance());
```

### 6.2 每帧更新

```cpp
// 主循环
while (!window.ShouldClose()) {
    // 1. 计算 DeltaTime
    float deltaTime = ...;
    
    // 2. 更新输入
    inputManager.Tick();
    
    // 3. 更新场景逻辑（包括所有 Component::Update）
    Scene->Tick(deltaTime);
    
    // 4. 提取渲染数据
    std::vector<RenderPacket> packets;
    Scene->ExtractRenderPackets(packets);
    
    // 5. 渲染
    renderer.Render(viewData, packets, uiCallback);
}
```

## 7. 设计优势

| 优势 | 说明 |
|------|------|
| **灵活性** | 功能可以任意组合，不受继承树限制 |
| **可扩展性** | 添加新功能只需创建新 Component |
| **性能友好** | 数据局部性好，便于缓存优化 |
| **并行友好** | Component 更新可并行化（未来扩展） |
| **逻辑清晰** | 每个 Component 职责单一 |
