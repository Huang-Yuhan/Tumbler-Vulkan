add_executable(App-Tumbler main.cpp)

target_link_libraries(App-Tumbler PRIVATE TumblerCore)
target_compile_features(App-Tumbler PRIVATE cxx_std_20)

# Windows: 运行时 DLL 复制
if(WIN32)
    add_custom_command(TARGET App-Tumbler POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:App-Tumbler>
            $<TARGET_FILE_DIR:App-Tumbler>
        COMMAND_EXPAND_LISTS
    )
endif()

# 复制 assets 目录到可执行文件目录
add_custom_command(TARGET App-Tumbler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:App-Tumbler>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/engine.json"
        "$<TARGET_FILE_DIR:App-Tumbler>/engine.json"
)
