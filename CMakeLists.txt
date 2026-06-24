cmake_minimum_required(VERSION 3.28)
project(Tumbler-Engine LANGUAGES CXX)

# 强制要求使用 MSVC 编译器
if(WIN32 AND NOT MSVC)
    message(FATAL_ERROR
            "本项目必须使用 MSVC 编译器编译。请在 CLion Toolchain 中切换为 Visual Studio 并重新 Reload。\n"
            "English: This project requires the MSVC compiler. In CLion, switch Toolchain to Visual Studio and reload CMake.")
endif()

# C++26 开关：Windows MSVC 若对 C++26 支持不佳可设 OFF 回退 C++20
option(TUMBLER_USE_CPP26 "Use C++26 features (requires GCC 14+ / MSVC 17.12+)" ON)

if(TUMBLER_USE_CPP26)
    if(MSVC)
        set(CMAKE_CXX_STANDARD 23)          # CMake 还不认识 26，MSVC 通过 c++latest 拿到
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
    else()
        set(CMAKE_CXX_STANDARD 26)
    endif()
    message(STATUS "C++ Standard: C++26 (MSVC: c++latest)")
else()
    set(CMAKE_CXX_STANDARD 20)
    message(STATUS "C++ Standard: C++20 (C++26 disabled via TUMBLER_USE_CPP26=OFF)")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest)
option(TUMBLER_ENABLE_RUNTIME_SMOKE_TESTS "Enable runtime smoke tests that launch App-Tumbler." OFF)

# 在 CMakeLists.txt 顶部
if(MSVC)
    add_compile_options("/utf-8")
endif()

# 1. 查找依赖
find_package(Vulkan REQUIRED)
if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    set(TUMBLER_VCPKG_GLFW3_DIR
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/glfw3")
    if(EXISTS "${TUMBLER_VCPKG_GLFW3_DIR}/glfw3Config.cmake")
        set(glfw3_DIR "${TUMBLER_VCPKG_GLFW3_DIR}" CACHE PATH
                "Path to the glfw3 CMake package config" FORCE)
    endif()
endif()
find_package(glfw3 3.4 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_path(Stb_INCLUDE_DIR
        NAMES stb_image.h
)
if(NOT Stb_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find stb headers (stb_image.h).")
endif()
find_package(imgui CONFIG REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)
find_package(slang CONFIG REQUIRED)
if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
endif()

# 统一 vcpkg 路径，供测试子目录复用（避免重复的 _vcpkg_triplet 回退逻辑）
if(DEFINED VCPKG_TARGET_TRIPLET)
    set(TUMBLER_VCPKG_TRIPLET "${VCPKG_TARGET_TRIPLET}" CACHE STRING "vcpkg triplet for this build")
elseif(WIN32)
    set(TUMBLER_VCPKG_TRIPLET "x64-windows" CACHE STRING "vcpkg triplet for this build")
elseif(APPLE)
    set(TUMBLER_VCPKG_TRIPLET "arm64-osx" CACHE STRING "vcpkg triplet for this build")
else()
    set(TUMBLER_VCPKG_TRIPLET "x64-linux" CACHE STRING "vcpkg triplet for this build")
endif()

if(DEFINED VCPKG_INSTALLED_DIR)
    set(TUMBLER_VCPKG_INSTALLED_DIR "${VCPKG_INSTALLED_DIR}" CACHE STRING "vcpkg installed packages root")
else()
    set(TUMBLER_VCPKG_INSTALLED_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed" CACHE STRING "vcpkg installed packages root")
endif()

# 2. 编译 Shader (跟之前一样)
add_subdirectory(assets/shaders)

# =========================================================
# 3. 构建核心引擎库 (关键步骤)
# =========================================================
# 收集核心源码
# 加上 CONFIGURE_DEPENDS，这样你新建 cpp 文件时，CMake 会自动重新加载
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/Core/*.cpp")


# 创建一个静态库 "TumblerCore"
# STATIC 表示编译为 .lib 或 .a 文件
add_library(TumblerCore STATIC ${CORE_SOURCES})

# 配置库的头文件路径
# PUBLIC 关键字很重要：这意味着链接了这个库的程序（如 rectangle），
# 也会自动获得这些 Include 路径，不需要再写一遍。
target_include_directories(TumblerCore PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        ${Stb_INCLUDE_DIR}
)

# 配置库的链接依赖
# PUBLIC 表示链接了 TumblerCore 的程序，也会自动链接 Vulkan/GLFW/GLM
target_link_libraries(TumblerCore PUBLIC
        Vulkan::Vulkan
        glfw
        glm::glm
        GPUOpen::VulkanMemoryAllocator
        spdlog::spdlog
        imgui::imgui
        tinyobjloader::tinyobjloader
        slang::slang
)

# 平台适配
if (WIN32)
    target_link_libraries(TumblerCore PUBLIC winmm)
elseif (APPLE)
    target_link_libraries(TumblerCore PUBLIC "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

# =========================================================
# 4. 资产导入工具
# =========================================================
add_subdirectory(src/Tools/AssetImporter)

# =========================================================
# 5. 添加例子子目录
# =========================================================
add_subdirectory(src/Examples)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(BUILD_TESTING)
    # Only register the runtime smoke test when explicitly enabled.
    # This avoids CI/test failures on machines that cannot launch the app.
    if(NOT TUMBLER_ENABLE_RUNTIME_SMOKE_TESTS)
        message(STATUS "TUMBLER_ENABLE_RUNTIME_SMOKE_TESTS is OFF; skipping Smoke.RuntimeArtifacts test")
    else()
        if(NOT DEFINED TUMBLER_SHADERS_COMPILED)
            set(TUMBLER_SHADERS_COMPILED OFF)
        endif()
        add_test(NAME Smoke.RuntimeArtifacts
                 COMMAND ${CMAKE_COMMAND}
                 -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
                 -DBINARY_DIR=${CMAKE_BINARY_DIR}
                 -DSHADERS_COMPILED=${TUMBLER_SHADERS_COMPILED}
                 -P ${CMAKE_SOURCE_DIR}/cmake/tests/SmokeRuntimeArtifacts.cmake)
    endif()
endif()
