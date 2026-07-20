# CLAUDE.md

Tumbler — Nanite-style GPU-Driven renderer. Single executable, Slang shaders, Vulkan 1.4 (Dynamic Rendering).

## Build (Linux)

```bash
cmake -S . -B build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DBUILD_TESTING=ON
cmake --build build-linux
```

## Run

```bash
./build-linux/Tumbler assets/scenes/demo.tscene
```

## Architecture

```
src/
├── Core/          — 平台无关基础设施
│   ├── Math/      — Frustum, BoundingSphere (glm 没有的)
│   ├── Platform/  — AppWindow (GLFW)
│   └── Utils/     — Log, Assert
├── Gfx/           — Vulkan 基础设施（不关心渲染内容）
├── Render/        — GPU-Driven 渲染管线
├── Assets/        — 场景和资产加载
├── UI/            — ImGui 调试面板
└── main.cpp       — 入口
```

shaders/ — Slang 源码，CMake 自动编译为 SPIR-V

## Key Decisions

- Vulkan 1.4 → Dynamic Rendering (vkCmdBeginRendering) core
- Timeline Semaphore based DeletionQueue for GPU resource lifetime
- MAX_FRAMES_IN_FLIGHT = 2, ring-buffer per-frame resources
- GpuBuffer/GpuImage RAII → destructor enqueues to DeletionQueue
- glm for vector/matrix math, Tumbler math only for Frustum/BoundingSphere
- Slang for all shaders (no GLSL)
- Single executable, scene JSON as CLI argument
- ImGui for debug panel (visible clusters, triangle count)
- No ECS, no editor, no asset importer (for now)
