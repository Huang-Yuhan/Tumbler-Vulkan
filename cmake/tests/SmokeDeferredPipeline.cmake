cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED SOURCE_DIR OR NOT DEFINED BINARY_DIR)
    message(FATAL_ERROR "SOURCE_DIR and BINARY_DIR must be provided to SmokeDeferredPipeline.cmake")
endif()

function(assert_exists path label)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "${label} not found: ${path}")
    endif()
endfunction()

function(assert_contains file_path regex label)
    assert_exists("${file_path}" "${label} file")
    file(READ "${file_path}" file_content)
    string(REGEX MATCH "${regex}" match_result "${file_content}")
    if(match_result STREQUAL "")
        message(FATAL_ERROR "${label} check failed in ${file_path}. Missing pattern: ${regex}")
    endif()
endfunction()

set(required_deferred_shaders
        "${SOURCE_DIR}/assets/shaders/engine/deferred_geometry.frag"
        "${SOURCE_DIR}/assets/shaders/engine/deferred_lighting.vert"
        "${SOURCE_DIR}/assets/shaders/engine/deferred_lighting.frag"
)

set(skip_spirv_checks FALSE)
if(DEFINED SHADERS_COMPILED AND NOT SHADERS_COMPILED)
    set(skip_spirv_checks TRUE)
    message(STATUS "Skipping deferred SPIR-V checks because glslc is unavailable in this environment.")
endif()

foreach(shader_file IN LISTS required_deferred_shaders)
    assert_exists("${shader_file}" "Deferred shader source")
    if(NOT skip_spirv_checks)
        assert_exists("${shader_file}.spv" "Deferred shader SPIR-V")
    endif()
endforeach()

set(deferred_pipeline_cpp "${SOURCE_DIR}/src/Core/Graphics/Pipelines/FDeferredPipeline.cpp")
assert_contains("${deferred_pipeline_cpp}" "void FDeferredPipeline::RecreateResources\\(VulkanRenderer\\* renderer\\)" "Deferred recreate entry")
assert_contains("${deferred_pipeline_cpp}" "VkSubpassDescription subpass0" "Deferred geometry subpass")
assert_contains("${deferred_pipeline_cpp}" "VkSubpassDescription subpass1" "Deferred lighting subpass")
assert_contains("${deferred_pipeline_cpp}" "UpdateLightingDescriptorSet\\(renderer\\);" "Deferred descriptor refresh hook")
assert_contains("${deferred_pipeline_cpp}" "VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL" "Deferred input attachment read layout")

set(vulkan_renderer_cpp "${SOURCE_DIR}/src/Core/Graphics/VulkanRenderer.cpp")
assert_contains("${vulkan_renderer_cpp}" "std::make_unique<FDeferredPipeline>" "Deferred pipeline wiring")
assert_contains("${vulkan_renderer_cpp}" "FlushPendingDescriptorSetFrees\\(\\);" "Descriptor deferred-free flush hook")

message(STATUS "Smoke checks passed for deferred pipeline artifacts and integration hooks.")
