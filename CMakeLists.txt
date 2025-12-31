cmake_minimum_required(VERSION 3.31)
project(VulkanApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译器标志
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -Wextra")
endif()

# 查找 Vulkan（必需）
find_package(Vulkan REQUIRED)

# 查找 GLFW（必需）
find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    pkg_check_modules(GLFW REQUIRED glfw3)
else()
    # 如果 pkg-config 不可用，尝试其他方式查找 GLFW
    find_path(GLFW_INCLUDE_DIR NAMES GLFW/glfw3.h)
    find_library(GLFW_LIBRARY NAMES glfw3 glfw)

    if (GLFW_INCLUDE_DIR AND GLFW_LIBRARY)
        set(GLFW_FOUND TRUE)
        set(GLFW_INCLUDE_DIRS ${GLFW_INCLUDE_DIR})
        set(GLFW_LIBRARIES ${GLFW_LIBRARY})
    else()
        message(FATAL_ERROR "GLFW not found. Please install GLFW development packages")
    endif()
endif()

# 查找 GLM（头文件库）
find_path(GLM_INCLUDE_DIR NAMES glm/glm.hpp)
if (NOT GLM_INCLUDE_DIR)
    message(FATAL_ERROR "GLM not found. Please install GLM development packages")
endif()

# 创建可执行文件
add_executable(VulkanApp main.cpp)

# 包含头文件目录
target_include_directories(VulkanApp PRIVATE
        ${Vulkan_INCLUDE_DIRS}
        ${GLFW_INCLUDE_DIRS}
        ${GLM_INCLUDE_DIR}
)

# 链接库
target_link_libraries(VulkanApp PRIVATE
        ${Vulkan_LIBRARIES}
        ${GLFW_LIBRARIES}
)

# 特定平台的额外链接
if (WIN32)
    target_link_libraries(VulkanApp PRIVATE
            winmm
    )
elseif (APPLE)
    target_link_libraries(VulkanApp PRIVATE
            -framework Cocoa
            -framework IOKit
            -framework CoreVideo
    )
else()  # Linux/Unix
    target_link_libraries(VulkanApp PRIVATE
            X11
    )
endif()

# Windows: 复制 Vulkan DLL 到输出目录
if (WIN32 AND Vulkan_LIBRARIES)
    get_filename_component(VULKAN_DLL_DIR ${Vulkan_LIBRARIES} DIRECTORY)
    add_custom_command(TARGET VulkanApp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VULKAN_DLL_DIR}/vulkan-1.dll"
            $<TARGET_FILE_DIR:VulkanApp>
            COMMENT "Copying Vulkan DLL to output directory"
    )
endif()

# 验证找到的包
message(STATUS "Vulkan found: ${Vulkan_FOUND}")
message(STATUS "GLFW found: ${GLFW_FOUND}")
message(STATUS "GLM found: ${GLM_INCLUDE_DIR}")
