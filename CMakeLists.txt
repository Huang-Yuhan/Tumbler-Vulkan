cmake_minimum_required(VERSION 3.28)
project(Tumbler LANGUAGES CXX)

# ---- Compiler / Platform ----
if(WIN32 AND NOT MSVC)
    message(FATAL_ERROR "本项目必须使用 MSVC 编译器。")
endif()
if(MSVC)
    add_compile_options("/utf-8")
endif()

# ---- C++ Standard ----
option(TUMBLER_USE_CPP20 "Use C++20 instead of C++26" OFF)
if(TUMBLER_USE_CPP20)
    set(CMAKE_CXX_STANDARD 20)
else()
    if(MSVC)
        set(CMAKE_CXX_STANDARD 23)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
    else()
        set(CMAKE_CXX_STANDARD 26)
    endif()
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Testing ----
include(CTest)
option(BUILD_TESTING "Build tests" ON)

# ---- Dependencies (vcpkg) ----
find_package(Vulkan REQUIRED)
find_package(glfw3 3.4 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(slang CONFIG REQUIRED)
find_package(metis CONFIG REQUIRED)
if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
endif()

find_path(Stb_INCLUDE_DIR NAMES stb_image.h)
if(NOT Stb_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find stb headers.")
endif()

# ---- Slang Shader Compilation ----
file(GLOB_RECURSE SHADER_SOURCES "${CMAKE_SOURCE_DIR}/shaders/*.slang")
set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")

# Locate slangc (vcpkg puts it in tools/shader-slang, fallback to PATH)
find_program(SLANGC_EXECUTABLE slangc
    PATHS "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/tools/shader-slang"
    NO_DEFAULT_PATH
)
if(NOT SLANGC_EXECUTABLE)
    find_program(SLANGC_EXECUTABLE slangc)
endif()
message(STATUS "slangc: ${SLANGC_EXECUTABLE}")

# ---- nanite_debug.slang (multi-entry-point: vert_main + frag_main) ----
set(NANITE_VERT "${SHADER_OUTPUT_DIR}/nanite_debug_vert.spv")
set(NANITE_FRAG "${SHADER_OUTPUT_DIR}/nanite_debug_frag.spv")

add_custom_command(
    OUTPUT ${NANITE_VERT}
    COMMAND ${SLANGC_EXECUTABLE} ${CMAKE_SOURCE_DIR}/shaders/nanite_debug.slang
            -entry vert_main -stage vertex -o ${NANITE_VERT} -target spirv
    DEPENDS ${CMAKE_SOURCE_DIR}/shaders/nanite_debug.slang
    COMMENT "Compiling nanite_debug.slang (vert) → SPIR-V"
)
add_custom_command(
    OUTPUT ${NANITE_FRAG}
    COMMAND ${SLANGC_EXECUTABLE} ${CMAKE_SOURCE_DIR}/shaders/nanite_debug.slang
            -entry frag_main -stage fragment -o ${NANITE_FRAG} -target spirv
    DEPENDS ${CMAKE_SOURCE_DIR}/shaders/nanite_debug.slang
    COMMENT "Compiling nanite_debug.slang (frag) → SPIR-V"
)
list(APPEND SPV_OUTPUTS ${NANITE_VERT} ${NANITE_FRAG})

# ---- Generic shader compilation (single entry point "main") ----
foreach(shader ${SHADER_SOURCES})
    get_filename_component(shader_name ${shader} NAME_WE)
    set(spv_output "${SHADER_OUTPUT_DIR}/${shader_name}.spv")

    # Detect stage from suffix: name_vert → vertex, name_frag → fragment
    if(shader_name MATCHES "_vert$")
        set(stage_flag -stage vertex)
    elseif(shader_name MATCHES "_frag$")
        set(stage_flag -stage fragment)
    elseif(shader_name MATCHES "_comp$")
        set(stage_flag -stage compute)
    else()
        set(stage_flag "")
    endif()

    # Skip nanite_debug.slang (already handled above)
    if(shader_name STREQUAL "nanite_debug")
        continue()
    endif()

    add_custom_command(
        OUTPUT ${spv_output}
        COMMAND ${SLANGC_EXECUTABLE} ${shader} -o ${spv_output} -target spirv ${stage_flag}
        DEPENDS ${shader}
        COMMENT "Compiling ${shader_name}.slang → SPIR-V"
    )
    list(APPEND SPV_OUTPUTS ${spv_output})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPV_OUTPUTS})

# ---- Main Executable ----
file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")
add_executable(Tumbler ${ALL_SOURCES})

target_include_directories(Tumbler PRIVATE "${CMAKE_SOURCE_DIR}/src" ${Stb_INCLUDE_DIR})

target_link_libraries(Tumbler PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    GPUOpen::VulkanMemoryAllocator
    spdlog::spdlog
    imgui::imgui
    tinyobjloader::tinyobjloader
    nlohmann_json::nlohmann_json
    slang::slang
    metis
)

add_dependencies(Tumbler Shaders)

target_compile_definitions(Tumbler PRIVATE
    SHADER_DIR="${SHADER_OUTPUT_DIR}"
    ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
)

if(WIN32)
    target_link_libraries(Tumbler PRIVATE winmm)
endif()

# ---- Tests ----
if(BUILD_TESTING AND EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
