cmake_minimum_required(VERSION 3.28)
project(Tumbler-Engine LANGUAGES CXX)

# 强制要求使用 MSVC 编译器
if(WIN32 AND NOT MSVC)
    message(FATAL_ERROR
            "本项目必须使用 MSVC 编译器编译。请在 CLion Toolchain 中切换为 Visual Studio 并重新 Reload。\n"
            "English: This project requires the MSVC compiler. In CLion, switch Toolchain to Visual Studio and reload CMake.")
endif()

set(CMAKE_CXX_STANDARD 20)
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
if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
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
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
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
)

# 平台适配
if (WIN32)
    target_link_libraries(TumblerCore PUBLIC winmm)
elseif (APPLE)
    target_link_libraries(TumblerCore PUBLIC "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

# =========================================================
# 4. 添加例子子目录
# =========================================================
# CMake 会进入这个文件夹寻找另一个 CMakeLists.txt
add_subdirectory(src/Examples)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(BUILD_TESTING)
    add_test(
            NAME Smoke.RuntimeArtifacts
            COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DBINARY_DIR=${CMAKE_BINARY_DIR}
            -DSHADERS_COMPILED=${TUMBLER_SHADERS_COMPILED}
            -P ${CMAKE_SOURCE_DIR}/cmake/tests/SmokeRuntimeArtifacts.cmake
    )

    add_test(
            NAME Smoke.DeferredPipeline
            COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DBINARY_DIR=${CMAKE_BINARY_DIR}
            -DSHADERS_COMPILED=${TUMBLER_SHADERS_COMPILED}
            -P ${CMAKE_SOURCE_DIR}/cmake/tests/SmokeDeferredPipeline.cmake
    )
endif()
