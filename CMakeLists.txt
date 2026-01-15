cmake_minimum_required(VERSION 3.31)
project(Tumbler-Engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# 1. 查找依赖
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# 2. 编译 Shader (跟之前一样)
add_subdirectory(assets/shaders)

# =========================================================
# 3. 构建核心引擎库 (关键步骤)
# =========================================================
# 收集核心源码
file(GLOB_RECURSE CORE_SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")

# =========================================================
# 2. 收集 src/examples 下的 .cpp 文件 (这是我们要排除的黑名单)
# =========================================================
file(GLOB_RECURSE EXCLUDE_SOURCES "${CMAKE_SOURCE_DIR}/src/examples/*.cpp")

# =========================================================
# 3. 从核心源码列表中移除 examples 的文件
# =========================================================
if (EXCLUDE_SOURCES)
    list(REMOVE_ITEM CORE_SOURCES ${EXCLUDE_SOURCES})
endif()

# 创建一个静态库 "TumblerCore"
# STATIC 表示编译为 .lib 或 .a 文件
add_library(TumblerCore STATIC ${CORE_SOURCES})

# 配置库的头文件路径
# PUBLIC 关键字很重要：这意味着链接了这个库的程序（如 rectangle），
# 也会自动获得这些 Include 路径，不需要再写一遍。
target_include_directories(TumblerCore PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# 配置库的链接依赖
# PUBLIC 表示链接了 TumblerCore 的程序，也会自动链接 Vulkan/GLFW/GLM
target_link_libraries(TumblerCore PUBLIC
        Vulkan::Vulkan
        glfw
        glm::glm
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
add_subdirectory(src/examples)