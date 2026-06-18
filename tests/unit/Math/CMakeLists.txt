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

# Win32 平台：DLL 拷贝 + 环境变量；Linux：LD_LIBRARY_PATH
if(WIN32)
    add_custom_command(TARGET TumblerMathTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:TumblerMathTests>
            $<TARGET_FILE_DIR:TumblerMathTests>
            COMMAND_EXPAND_LISTS
    )

    set(_env_mod_list
            "PATH=path_list_prepend:${TUMBLER_VCPKG_INSTALLED_DIR}/${TUMBLER_VCPKG_TRIPLET}/debug/bin"
            "PATH=path_list_prepend:${TUMBLER_VCPKG_INSTALLED_DIR}/${TUMBLER_VCPKG_TRIPLET}/bin"
    )
    list(JOIN _env_mod_list ";" _env_mod)
    set_tests_properties(${TumblerMathTests_TESTS}
            PROPERTIES
            ENVIRONMENT_MODIFICATION "${_env_mod}"
    )
elseif(UNIX AND NOT APPLE)
    set_tests_properties(${TumblerMathTests_TESTS}
            PROPERTIES
            ENVIRONMENT_MODIFICATION
                "LD_LIBRARY_PATH=path_list_prepend:${TUMBLER_VCPKG_INSTALLED_DIR}/${TUMBLER_VCPKG_TRIPLET}/lib"
    )
endif()
