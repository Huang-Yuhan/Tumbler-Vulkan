file(GLOB_RECURSE GRAPHICS_TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(TumblerGraphicsPhase3Tests
        ${GRAPHICS_TEST_SOURCES}
)

target_link_libraries(TumblerGraphicsPhase3Tests PRIVATE
        TumblerCore
        GTest::gtest_main
)

target_compile_features(TumblerGraphicsPhase3Tests PRIVATE cxx_std_20)
target_compile_definitions(TumblerGraphicsPhase3Tests PRIVATE
        TUMBLER_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

include(GoogleTest)
gtest_add_tests(
        TARGET TumblerGraphicsPhase3Tests
        TEST_LIST TumblerGraphicsPhase3Tests_TESTS
)

set_tests_properties(${TumblerGraphicsPhase3Tests_TESTS}
        PROPERTIES LABELS "unit;vulkan"
)

if(WIN32)
    set(_vcpkg_triplet "${VCPKG_TARGET_TRIPLET}")
    if(NOT _vcpkg_triplet)
        set(_vcpkg_triplet "x64-windows")
    endif()
    if(_vcpkg_triplet MATCHES "^\\$env:")
        set(_vcpkg_triplet "x64-windows")
    endif()

    set(_vcpkg_installed_dir "${CMAKE_BINARY_DIR}/vcpkg_installed")
    if(DEFINED VCPKG_INSTALLED_DIR)
        set(_vcpkg_installed_dir "${VCPKG_INSTALLED_DIR}")
    endif()

    add_custom_command(TARGET TumblerGraphicsPhase3Tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:TumblerGraphicsPhase3Tests>
            $<TARGET_FILE_DIR:TumblerGraphicsPhase3Tests>
            COMMAND_EXPAND_LISTS
    )

    set(_graphics_test_env_mod_list
            "PATH=path_list_prepend:${_vcpkg_installed_dir}/${_vcpkg_triplet}/debug/bin"
            "PATH=path_list_prepend:${_vcpkg_installed_dir}/${_vcpkg_triplet}/bin"
    )
    list(JOIN _graphics_test_env_mod_list ";" _graphics_test_env_mod)

    set_tests_properties(${TumblerGraphicsPhase3Tests_TESTS}
            PROPERTIES
            ENVIRONMENT_MODIFICATION "${_graphics_test_env_mod}"
    )
endif()
