# shaders/CMakeLists.txt

# 1. 查找编译器
find_program(GLSLC_EXECUTABLE NAMES glslc HINTS "$ENV{VULKAN_SDK}/bin")
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "Could not find glslc shader compiler!")
endif()

# 2. 递归查找所有 shader 源码
# CMAKE_CURRENT_SOURCE_DIR 现在就是 shaders 文件夹
file(GLOB_RECURSE SHADER_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/*.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.comp"
)

set(SPIRV_BINARIES "")

foreach(SOURCE_FILE ${SHADER_SOURCES})
    # 计算相对路径 (例如: 从 "path/shaders/ui/panel.frag" -> "ui/panel.frag")
    file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}" "${SOURCE_FILE}")

    # 设置输出文件
    # CMAKE_CURRENT_BINARY_DIR 会自动对应 build 目录下的 shaders 文件夹
    set(OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/${REL_PATH}.spv")

    # 确保输出子目录存在
    get_filename_component(OUTPUT_DIR "${OUTPUT_FILE}" DIRECTORY)
    file(MAKE_DIRECTORY ${OUTPUT_DIR})

    # 添加编译命令
    add_custom_command(
            OUTPUT ${OUTPUT_FILE}
            COMMAND ${GLSLC_EXECUTABLE} ${SOURCE_FILE} -o ${OUTPUT_FILE}
            DEPENDS ${SOURCE_FILE}
            COMMENT "Compiling shader: ${REL_PATH}"
    )

    list(APPEND SPIRV_BINARIES ${OUTPUT_FILE})
endforeach()

# 3. 定义 Target
# 注意：Target 名称在整个项目中必须唯一
add_custom_target(Shaders ALL DEPENDS ${SPIRV_BINARIES})