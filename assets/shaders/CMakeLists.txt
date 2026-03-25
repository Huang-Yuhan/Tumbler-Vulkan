# shaders/CMakeLists.txt

# 1. 查找编译器 (修改为更灵活的查找方式)
find_program(GLSLC_EXECUTABLE NAMES glslc HINTS "$ENV{VULKAN_SDK}/bin" "/usr/bin" "/usr/local/bin")
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "Could not find glslc shader compiler!")
endif()

# =========================================================
# 2. 定义路径 (关键修改)
# =========================================================
# 不再查找当前目录，而是查找项目根目录下的 assets/shaders
set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/assets/shaders")

# 递归查找所有 shader 源码
# 加上 CONFIGURE_DEPENDS，让 CMake 自动监控 Shader 文件的增减
file(GLOB_RECURSE SHADER_SOURCES CONFIGURE_DEPENDS
        "${SHADER_SOURCE_DIR}/*.vert"
        "${SHADER_SOURCE_DIR}/*.frag"
        "${SHADER_SOURCE_DIR}/*.comp"
)

set(SPIRV_BINARIES "")

foreach(SOURCE_FILE ${SHADER_SOURCES})
    # =========================================================
    # 3. 设置输出路径 (关键修改)
    # =========================================================
    # 原来的写法是输出到 build 目录，现在改为直接输出到源文件旁边 (.spv)
    # 这样后续的 copy_directory assets 才能把 .spv 一起带走
    set(OUTPUT_FILE "${SOURCE_FILE}.spv")

    # 获取文件名用于显示日志
    get_filename_component(FILE_NAME ${SOURCE_FILE} NAME)

    # 添加编译命令
    add_custom_command(
            OUTPUT ${OUTPUT_FILE}
            COMMAND ${GLSLC_EXECUTABLE} ${SOURCE_FILE} -o ${OUTPUT_FILE}
            DEPENDS ${SOURCE_FILE}
            COMMENT "Compiling shader: ${FILE_NAME} -> ${FILE_NAME}.spv"
    )

    list(APPEND SPIRV_BINARIES ${OUTPUT_FILE})
endforeach()

# 4. 定义 Target
add_custom_target(Shaders ALL DEPENDS ${SPIRV_BINARIES})