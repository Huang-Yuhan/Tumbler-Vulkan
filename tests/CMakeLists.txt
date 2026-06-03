add_executable(TumblerUnitTests
        unit/MathTests.cpp
)

target_link_libraries(TumblerUnitTests PRIVATE
        TumblerCore
        GTest::gtest_main
)

target_compile_features(TumblerUnitTests PRIVATE cxx_std_20)

include(GoogleTest)
gtest_add_tests(
        TARGET TumblerUnitTests
        TEST_LIST TumblerUnitTests_TESTS
)

if(WIN32)
    set(_vcpkg_triplet "${VCPKG_TARGET_TRIPLET}")
    if(NOT _vcpkg_triplet)
        set(_vcpkg_triplet "x64-windows")
    endif()
    if(_vcpkg_triplet MATCHES "^\\$env:")
        set(_vcpkg_triplet "x64-windows")
    endif()

    add_custom_command(TARGET TumblerUnitTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:TumblerUnitTests>
            $<TARGET_FILE_DIR:TumblerUnitTests>
            COMMAND_EXPAND_LISTS
    )

    set(_unit_test_env_mod_list
            "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/vcpkg_installed/${_vcpkg_triplet}/debug/bin"
            "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/vcpkg_installed/${_vcpkg_triplet}/bin"
    )
    list(JOIN _unit_test_env_mod_list ";" _unit_test_env_mod)

    set_tests_properties(${TumblerUnitTests_TESTS}
            PROPERTIES
            LABELS "unit"
            ENVIRONMENT_MODIFICATION "${_unit_test_env_mod}"
    )
else()
    set_tests_properties(${TumblerUnitTests_TESTS}
            PROPERTIES LABELS "unit"
    )
endif()
