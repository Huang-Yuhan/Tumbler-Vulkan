# Testing and CI Guide

This document summarizes how to run tests locally, what CI validates, and how to add new unit tests safely.

## 1. Build with Tests Enabled

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DBUILD_TESTING=ON
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

Note: shader smoke checks rely on `.spv` artifacts. If `glslc` is unavailable in CI, shader artifact assertions are skipped by design.

## 4. Add a New Unit Test

1. Add a `TEST(...)` case into one of the existing files under `tests/unit/`.
2. Keep tests deterministic.
3. Reconfigure if needed, then run:

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
