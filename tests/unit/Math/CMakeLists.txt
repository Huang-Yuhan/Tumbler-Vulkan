file(GLOB_RECURSE MATH_TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(TumblerMathTests
        ${MATH_TEST_SOURCES}
)

target_link_libraries(TumblerMathTests PRIVATE
        TumblerCore
        GTest::gtest_main
)

target_compile_features(TumblerMathTests PRIVATE cxx_std_20)

include(GoogleTest)
gtest_add_tests(
        TARGET TumblerMathTests
        TEST_LIST TumblerMathTests_TESTS
)

set_tests_properties(${TumblerMathTests_TESTS}
        PROPERTIES LABELS "unit"
)

# Win32 平台：DLL 拷贝 + 环境变量
if(WIN32)
    set(_vcpkg_triplet "${VCPKG_TARGET_TRIPLET}")
    if(NOT _vcpkg_triplet)
        set(_vcpkg_triplet "x64-windows")
    endif()
    if(_vcpkg_triplet MATCHES "^\\$env:")
        set(_vcpkg_triplet "x64-windows")
    endif()

    add_custom_command(TARGET TumblerMathTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:TumblerMathTests>
            $<TARGET_FILE_DIR:TumblerMathTests>
            COMMAND_EXPAND_LISTS
    )

    set(_unit_test_env_mod_list
            "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/vcpkg_installed/${_vcpkg_triplet}/debug/bin"
            "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/vcpkg_installed/${_vcpkg_triplet}/bin"
    )
    list(JOIN _unit_test_env_mod_list ";" _unit_test_env_mod)

    set_tests_properties(${TumblerMathTests_TESTS}
            PROPERTIES
            ENVIRONMENT_MODIFICATION "${_unit_test_env_mod}"
    )
endif()
