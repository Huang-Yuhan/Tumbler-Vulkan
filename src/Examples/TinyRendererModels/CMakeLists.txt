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

# =========================================================
# 2. 创建可执行文件
# =========================================================
add_executable(App-TinyRendererModels
        ${APP_SOURCES}
        ${APP_HEADERS}
)

# =========================================================
# 3. 链接与依赖
# =========================================================
target_link_libraries(App-TinyRendererModels PRIVATE TumblerCore)
if(TARGET Shaders)
    add_dependencies(App-TinyRendererModels Shaders)
endif()

# =========================================================
# 4. 资源拷贝
# =========================================================
add_custom_command(TARGET App-TinyRendererModels POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:App-TinyRendererModels>/assets"
        COMMENT "Copying assets to executable directory..."
)
