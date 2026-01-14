add_executable(App-Rectangle rectangle.cpp)
target_link_libraries(App-Rectangle PRIVATE TumblerCore)
add_dependencies(App-Rectangle Shaders) # 确保 Shader 编译先完成

# =========================================================
# [核心逻辑] 资源自动复制
# =========================================================
# 命令解释：
# POST_BUILD: 编译完成后执行
# copy_directory: 复制整个目录（如果目标存在，会覆盖更新）
# ${CMAKE_SOURCE_DIR}/assets: 你的源码目录下的 assets 文件夹
# $<TARGET_FILE_DIR:App-Rectangle>/assets: 编译出来的 exe 旁边的 assets 文件夹

add_custom_command(TARGET App-Rectangle POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:App-Rectangle>/assets"
        COMMENT "Copying assets to executable directory..."
)