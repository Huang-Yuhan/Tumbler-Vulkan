add_executable(App-Tumbler main.cpp)
target_link_libraries(App-Tumbler PRIVATE TumblerCore)
add_dependencies(App-Tumbler Shaders) # 确保 Shader 编译先完成



add_custom_command(TARGET App-Tumbler POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:App-Tumbler>/assets"
        COMMENT "Copying assets to executable directory..."
)