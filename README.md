# Tumbler Vulkan Engine

[![Windows CI](https://github.com/Huang-Yuhan/Tumbler-Vulkan/actions/workflows/windows-ci.yml/badge.svg?branch=main)](https://github.com/Huang-Yuhan/Tumbler-Vulkan/actions/workflows/windows-ci.yml)

A modern Vulkan game engine prototype with a component-based architecture, PBR workflow, and ImGui tooling.

## Status

- Core engine, examples, and CI pipeline are available.
- Main development plan: [Tumbler_Dev_Plan.md](Tumbler_Dev_Plan.md)

## Highlights

- Vulkan rendering pipeline
- PBR material workflow
- Component-style game architecture
- Runtime examples under `src/Examples`
- Unit tests with GoogleTest

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 (MSVC toolchain)
- CMake 3.28+
- vcpkg (`VCPKG_ROOT` set)
- Vulkan SDK/runtime

## Quick Start (Windows + MSVC + vcpkg)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DBUILD_TESTING=ON

cmake --build build --config Debug --parallel
```

For a full setup walkthrough, see [docs/00_Getting_Started.md](docs/00_Getting_Started.md).

## Test Commands

Run all tests:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

Run unit tests only:

```powershell
ctest --test-dir build -C Debug -L unit --output-on-failure
```

Smoke coverage includes:

- required runtime assets
- example executable outputs
- deferred pipeline integration checks
- shader artifact checks (`.spv`)

Note: if `glslc` is unavailable, shader compile checks are skipped in smoke tests (CI-safe behavior).

## CI

GitHub Actions workflow: [windows-ci.yml](.github/workflows/windows-ci.yml)

Current CI lane on push/PR to `main`:

- Configure (MSVC + vcpkg toolchain)
- Debug build
- `ctest --test-dir build -C Debug --output-on-failure`

## Encoding / Garbled Logs (Windows IDE)

If CLion/VSCode build output looks garbled, use:

- UTF-8 file/project encoding
- `VSLANG=1033` as stable English log fallback

Details: [docs/10_Troubleshooting_Guide.md](docs/10_Troubleshooting_Guide.md)

## Documentation

- [docs/INDEX.md](docs/INDEX.md)
- [docs/00_Getting_Started.md](docs/00_Getting_Started.md)
- [docs/01_Architecture_Overview.md](docs/01_Architecture_Overview.md)
- [docs/09_Rendering_Pipeline_Deep_Dive.md](docs/09_Rendering_Pipeline_Deep_Dive.md)
- [docs/10_Troubleshooting_Guide.md](docs/10_Troubleshooting_Guide.md)
- [docs/13_Testing_and_CI.md](docs/13_Testing_and_CI.md)

## Tech Stack

- Graphics API: Vulkan 1.3+
- Language: C++20
- Compiler: MSVC (Visual Studio 2022)
- Build system: CMake
- Package manager: vcpkg
