# 资源管理与依赖注入 (Asset Management & DI)

## 1. 为什么需要 AssetManager？
在初期的实现中，Mesh、Texture、Material 的加载散落在业务代码 (`AppLogic`) 和底层渲染器 (`VulkanRenderer::TextureManager`) 的各个角落。
- 加载的对象通常由强指针甚至 `vector` 内部隐式持有，容易产生内存泄漏。
- 同一个模型如果被实例化100次，可能会被从硬盘读取100次，导致巨大的内存和显存浪费。

因此，所有外部资产的加载、分配和存储汇总到了 `FAssetManager` 中。

## 2. 核心特性：自动缓存与按需分配
通过传入唯一的 ID 或文件路径（如 `GetOrLoadMesh("StingSword", "models/sword.obj")`）：
- **如果已经加载过**：直接返回同一份 `shared_ptr`。
- **如果是首次加载**：解析文件并在内存构建对象。
- **自动推送到 GPU**：由 `AssetManager` 调用 `Renderer->UploadMesh()` 或生成贴图，从业务代码中彻底隐藏底层的 Vulkan 操作细节。

## 3. 为什么拒绝 Singleton（单例模式）？
很多小型项目会将 `AssetManager` 写成全局 `static` 单例(`Get()`)。但在 Vulkan 等现代图形 API 中，这是极其危险的实践。

**原因：Vulkan 资源的销毁顺序限制极其严格。**
Vulkan 的所有对象（Buffer, Image, Pipeline, Layout）均隶属于逻辑设备 `VkDevice`。
如果 `AssetManager` 是全局静态变量：
- 它的析构往往发生在 `main()` 函数退出的**最后一刻**（比局部栈上的 `VulkanRenderer` 析构还要晚）。
- 等到 `AssetManager` 开始析构贴图和模型想要调用 `vkDestroyBuffer` 时，`VulkanRenderer` 早就把 `VkDevice` 给 `Destroy` 掉了，导致程序崩溃断言。

## 4. 依赖注入 (Dependency Injection) 的优雅
我们将 `FAssetManager` 做成普通的局部变量，严格规定在 `main()` 函数栈上的生命周期：
```cpp
// 1. 底层先建立
VulkanRenderer renderer;
renderer.Init(...);

// 2. 紧接着建立 AssetMgr 依赖 Renderer
FAssetManager assetManager;
assetManager.Initialize(&renderer);

// 3. 将这两个核心依赖通过指针注入给高层业务逻辑
AppLogic logic;
logic.Init(&renderer, &assetManager);
```
当程序结束时，依据 C++ 的局部变量逆序析构特性：
**Logic -> AssetManager -> VulkanRenderer**
确保每一张贴图都在设备被销毁前被正确释放。这是最严密安全的生命周期管理。
