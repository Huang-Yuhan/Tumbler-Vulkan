cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED SOURCE_DIR OR NOT DEFINED BINARY_DIR)
    message(FATAL_ERROR "SOURCE_DIR and BINARY_DIR must be provided to SmokeRuntimeArtifacts.cmake")
endif()

function(assert_any_exists label)
    set(found FALSE)
    foreach(candidate IN LISTS ARGN)
        if(EXISTS "${candidate}")
            set(found TRUE)
            break()
        endif()
    endforeach()

    if(NOT found)
        string(JOIN "\n  " checked_paths ${ARGN})
        message(FATAL_ERROR "${label} not found. Checked:\n  ${checked_paths}")
    endif()
endfunction()

set(required_assets
        "${SOURCE_DIR}/assets/models/Sting-Sword-lowpoly.obj"
        "${SOURCE_DIR}/assets/textures/white.png"
        "${SOURCE_DIR}/assets/textures/1.jpg"
)

set(missing_assets "")
foreach(asset IN LISTS required_assets)
    if(NOT EXISTS "${asset}")
        list(APPEND missing_assets "${asset}")
    endif()
endforeach()

if(missing_assets)
    string(JOIN "\n  " missing_asset_lines ${missing_assets})
    message(FATAL_ERROR "Missing required assets:\n  ${missing_asset_lines}")
endif()

file(GLOB_RECURSE shader_sources LIST_DIRECTORIES false
        "${SOURCE_DIR}/assets/shaders/*.vert"
        "${SOURCE_DIR}/assets/shaders/*.frag"
        "${SOURCE_DIR}/assets/shaders/*.comp"
)

set(missing_spv "")
foreach(shader_source IN LISTS shader_sources)
    if(NOT EXISTS "${shader_source}.spv")
        list(APPEND missing_spv "${shader_source}.spv")
    endif()
endforeach()

if(missing_spv)
    if(DEFINED SHADERS_COMPILED AND NOT SHADERS_COMPILED)
        message(STATUS "Skipping SPIR-V artifact presence check because glslc is unavailable in this environment.")
    else()
        string(JOIN "\n  " missing_spv_lines ${missing_spv})
        message(FATAL_ERROR
                "Missing compiled SPIR-V artifacts:\n  ${missing_spv_lines}\n"
                "Build the project (target Shaders) before running smoke tests.")
    endif()
endif()

if(WIN32)
    set(exe_ext ".exe")
else()
    set(exe_ext "")
endif()

set(config "$ENV{CTEST_CONFIGURATION_TYPE}")
if(config STREQUAL "")
    set(config "Debug")
endif()

set(app_tumbler_candidates
        "${BINARY_DIR}/src/Examples/Tumbler/${config}/App-Tumbler${exe_ext}"
        "${BINARY_DIR}/src/Examples/Tumbler/App-Tumbler${exe_ext}"
)

set(app_tiny_candidates
        "${BINARY_DIR}/src/Examples/TinyRendererModels/${config}/App-TinyRendererModels${exe_ext}"
        "${BINARY_DIR}/src/Examples/TinyRendererModels/App-TinyRendererModels${exe_ext}"
)

assert_any_exists("App-Tumbler executable" ${app_tumbler_candidates})
assert_any_exists("App-TinyRendererModels executable" ${app_tiny_candidates})

message(STATUS "Smoke checks passed for assets, shaders, and runtime executables.")
