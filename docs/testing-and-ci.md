# 测试与 CI

## 1. 构建时启用测试

```powershell
# Windows
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DBUILD_TESTING=ON

cmake --build build --config Debug --parallel
```

```bash
# Linux
cmake -S . -B build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DBUILD_TESTING=ON

cmake --build build-linux --target App-Tumbler
```

## 2. 运行测试

```powershell
# 全部测试
ctest --test-dir build -C Debug --output-on-failure

# 仅单元测试
ctest --test-dir build -C Debug -L unit --output-on-failure

# 按名称运行单个测试
ctest --test-dir build -C Debug -R "Scene" --output-on-failure

# 列出所有已注册测试
ctest --test-dir build -C Debug -N
```

## 3. 测试类型

- **Smoke 测试**: 验证关键构建产物（可执行文件、资产目录、Shader `.spv`）存在
- **Unit 测试** (GoogleTest): 纯 C++ 行为测试，链接 `TumblerCore`

### CMake 脚本烟雾测试

- `Smoke.RuntimeArtifacts` — 验证构建产物（exe、assets、shader SPV）
- `Smoke.DeferredPipeline` — 验证 Deferred 管线关键产物和钩子

### 添加新单元测试

1. 在 `tests/unit/` 下新建或修改文件
2. 在 `tests/CMakeLists.txt` 中注册
3. 保持测试确定性（无文件系统/网络依赖，无时序依赖）
4. 浮点断言使用 `EXPECT_NEAR`

```powershell
ctest --test-dir build -C Debug -L unit --output-on-failure
```

## 4. CI 流程 (GitHub Actions)

Workflow: `.github/workflows/windows-ci.yml`

1. Setup MSVC + bootstrap vcpkg
2. Install dependencies from `vcpkg.json`
3. CMake configure (`BUILD_TESTING=ON`)
4. Debug build
5. `ctest --test-dir build -C Debug --output-on-failure`
