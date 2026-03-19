# assets/models

将你的 OBJ 模型文件放在这里。
在 AppLogic 中通过以下方式加载：

```cpp
// 在 main.cpp 或 AppLogic 初始化后调用
auto mesh = appLogic.LoadOBJMesh("assets/models/your_model.obj", "MyModel");
renderer.UploadMesh(mesh.get());
```
