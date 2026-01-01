cmake_minimum_required(VERSION 3.31)

# 修改项目名
project(Tumbler-Vulkan LANGUAGES CXX)

# 修改 C++ 标准为 20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 1. 查找依赖（vcpkg 会自动引导这些 find_package 路径）
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# 2. 创建可执行文件
add_executable(Tumbler-Vulkan main.cpp)

# 3. 链接库（现代 CMake 方式）
# 链接目标会自动处理 INCLUDE_DIRECTORIES，所以你不需要手动写 target_include_directories
target_link_libraries(Tumbler-Vulkan PRIVATE
        Vulkan::Vulkan
        glfw
        glm::glm
)

# 4. 特定平台额外链接
if (WIN32)
    # Windows 下通常需要链接 winmm
    target_link_libraries(Tumbler-Vulkan PRIVATE winmm)
elseif (APPLE)
    target_link_libraries(Tumbler-Vulkan PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

# 验证状态打印
message(STATUS "Project: ${PROJECT_NAME} configured with vcpkg")
message(STATUS "Using Toolchain: ${CMAKE_TOOLCHAIN_FILE}")