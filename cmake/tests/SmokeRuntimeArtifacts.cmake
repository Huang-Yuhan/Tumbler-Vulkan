cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED SOURCE_DIR OR NOT DEFINED BINARY_DIR)
    message(FATAL_ERROR "SOURCE_DIR and BINARY_DIR must be provided to SmokeRuntimeArtifacts.cmake")
endif()

# 验证核心资产目录存在
set(required_dirs
        "${SOURCE_DIR}/assets"
        "${SOURCE_DIR}/assets/models"
        "${SOURCE_DIR}/assets/shaders"
        "${SOURCE_DIR}/assets/textures"
)

foreach(dir IN LISTS required_dirs)
    if(NOT IS_DIRECTORY "${dir}")
        message(FATAL_ERROR "Required directory not found: ${dir}")
    endif()
endforeach()

# 验证 GLSL 着色器源文件存在
set(required_shader_sources
        "${SOURCE_DIR}/assets/shaders/engine/pbr.vert"
        "${SOURCE_DIR}/assets/shaders/engine/pbr.frag"
)

foreach(shader IN LISTS required_shader_sources)
    if(NOT EXISTS "${shader}")
        message(FATAL_ERROR "Required shader source not found: ${shader}")
    endif()
endforeach()

message(STATUS "Smoke checks passed for required assets and shader sources.")
