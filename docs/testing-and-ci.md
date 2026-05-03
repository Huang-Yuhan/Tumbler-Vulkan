# Testing and CI Guide

This document summarizes how to run tests locally, what CI validates, and how to add new unit tests safely.

## 1. Build with Tests Enabled

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DBUILD_TESTING=ON `
  -DTUMBLER_ENABLE_RUNTIME_SMOKE_TESTS=ON
```

```powershell
cmake --build build --config Debug --parallel
```

## 2. Run Tests

Run all tests (smoke + unit):

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

Run unit tests only:

```powershell
ctest --test-dir build -C Debug -L unit --output-on-failure
```

List registered tests without running:

```powershell
ctest --test-dir build -C Debug -N
```

## 3. Test Types in This Repository

- `Smoke.*` tests: validate key assets, executable outputs, and deferred pipeline integration hooks.
- `unit` label tests: pure C++ behavior checks powered by GoogleTest.

### CMake 脚本烟雾测试（始终注册）

- `Smoke.RuntimeArtifacts` — 验证关键构建产物（exe、资产目录、着色器 `.spv`）存在
- `Smoke.DeferredPipeline` — 验证 Deferred 管线关键产物和钩子

### 运行时烟雾测试（需 `TUMBLER_ENABLE_RUNTIME_SMOKE_TESTS=ON`）

启动 `App-Tumbler` 自动执行固定帧数后退出：

- `Smoke.ResizeStressRuntime` — 连续切换窗口尺寸，验证 swapchain/UI Framebuffer 稳定性
- `Smoke.DescriptorStressRuntime` — 批量创建/销毁材质实例，验证描述符生命周期
- `Smoke.HiddenWindowRuntime` — 隐藏窗口渲染固定帧数，验证初始化/渲染/退出全链路

着色器产物检查依赖 `glslc`，不可用时自动跳过。

## 4. Add a New Unit Test

现有单元测试文件（`tests/unit/`）：

| 文件 | 覆盖内容 |
|------|----------|
| `DescriptorSetFreeQueueTests.cpp` | 描述符延迟释放队列 |
| `FQuaternionTests.cpp` | 四元数运算 |
| `CTransformTests.cpp` | Transform 层级变换 |
| `FActorTests.cpp` | Actor 创建/组件管理 |
| `FSceneTests.cpp` | 场景生命周期/延迟销毁 |

添加新测试：

1. 在对应文件中添加 `TEST(...)` 用例，或新建文件后在 `tests/CMakeLists.txt` 中注册
2. 保持测试确定性
3. 重新配置后运行：

Deterministic checklist:

- avoid filesystem/network dependence
- avoid timing-sensitive sleeps
- keep assertions strict but numerically stable (`EXPECT_NEAR` for float math)

```powershell
ctest --test-dir build -C Debug -N
ctest --test-dir build -C Debug -L unit --output-on-failure
```

If tests are not discovered, confirm `tests/CMakeLists.txt` includes the file and `BUILD_TESTING=ON` is set at configure time.

## 5. CI Overview (Windows)

Workflow file: `.github/workflows/windows-ci.yml`

Current CI stages:

1. Setup MSVC + bootstrap pinned vcpkg
2. Install dependencies from `vcpkg.json`
3. CMake configure (`BUILD_TESTING=ON`)
4. Debug build
5. `ctest --test-dir build -C Debug --output-on-failure`

Runtime note: CI prepends vcpkg runtime directories for test execution to avoid missing-DLL failures on Windows.
