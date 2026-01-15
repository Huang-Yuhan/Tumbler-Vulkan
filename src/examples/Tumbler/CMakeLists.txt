# =========================================================
# 1. 自动搜索当前目录下的 .h, .hpp, .cpp, .c 文件
# =========================================================
file(GLOB APP_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

file(GLOB APP_HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

# 建议：如果你的 CMake 版本 >= 3.12，加上 CONFIGURE_DEPENDS
# 这样当你新建文件时，CMake 会自动检测到并重新生成构建系统
# file(GLOB APP_SOURCES CONFIGURE_DEPENDS "*.cpp")

# =========================================================
# 2. 创建可执行文件 (把搜索到的变量放进去)
# =========================================================
# 注意：这里不需要再显式写 main.cpp 了，因为它已经被包含在 APP_SOURCES 里了
add_executable(App-Tumbler
        ${APP_SOURCES}
        ${APP_HEADERS}
)

# =========================================================
# 3. 链接与依赖 (保持不变)
# =========================================================
target_link_libraries(App-Tumbler PRIVATE TumblerCore)
add_dependencies(App-Tumbler Shaders) # 确保 Shader 编译先完成

# =========================================================
# 4. 资源拷贝 (保持不变)
# =========================================================
add_custom_command(TARGET App-Tumbler POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:App-Tumbler>/assets"
        COMMENT "Copying assets to executable directory..."
)