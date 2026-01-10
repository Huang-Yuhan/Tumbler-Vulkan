cmake_minimum_required(VERSION 3.31)
project(Tumbler-Vulkan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# 1. 依赖查找
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# 2. 添加 Shader 子目录 (我们之前做好的)
add_subdirectory(shaders)

# =========================================================
# 3. 收集源文件
# =========================================================
# 方式 A (推荐用于开发阶段)：递归查找 src 下所有 cpp
file(GLOB_RECURSE SOURCE_FILES "src/*.cpp")

# 方式 B (最严格，适合发布)：手动列出，防止意外编译
# set(SOURCE_FILES
#     src/main.cpp
#     src/Application.cpp
# )

# =========================================================
# 4. 创建可执行文件
# =========================================================
add_executable(Tumbler-Vulkan ${SOURCE_FILES})

# =========================================================
# 5. 配置 Include 路径 (关键步骤)
# =========================================================
# 告诉 CMake，头文件在 include 目录下
# PRIVATE 意味着这些路径只在这个目标内部使用
target_include_directories(Tumbler-Vulkan PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# =========================================================
# 6. 链接库
# =========================================================
target_link_libraries(Tumbler-Vulkan PRIVATE
        Vulkan::Vulkan
        glfw
        glm::glm
)

# 平台适配
if (WIN32)
    target_link_libraries(Tumbler-Vulkan PRIVATE winmm)
elseif (APPLE)
    target_link_libraries(Tumbler-Vulkan PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

add_dependencies(Tumbler-Vulkan Shaders)