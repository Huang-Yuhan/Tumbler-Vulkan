add_executable(TumblerUnitTests
        unit/DescriptorSetFreeQueueTests.cpp
        unit/FQuaternionTests.cpp
        unit/CTransformTests.cpp
        unit/FActorTests.cpp
        unit/FSceneTests.cpp
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
    # Keep runtime DLLs next to the test executable for direct invocation.
    add_custom_command(TARGET TumblerUnitTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:TumblerUnitTests>
            $<TARGET_FILE_DIR:TumblerUnitTests>
            COMMAND_EXPAND_LISTS
    )

    set(_vcpkg_triplet "${VCPKG_TARGET_TRIPLET}")
    if(NOT _vcpkg_triplet)
        set(_vcpkg_triplet "x64-windows")
    endif()
    if(_vcpkg_triplet MATCHES "^\\$env:")
        set(_vcpkg_triplet "x64-windows")
    endif()

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

if(TUMBLER_ENABLE_RUNTIME_SMOKE_TESTS AND TARGET App-Tumbler)
    add_test(
            NAME Smoke.ResizeStressRuntime
            COMMAND $<TARGET_FILE:App-Tumbler>
            --resize-stress-test
            --test-max-frames=240
            --test-frames-per-resize=20
    )

    add_test(
            NAME Smoke.DescriptorStressRuntime
            COMMAND $<TARGET_FILE:App-Tumbler>
            --descriptor-stress-test
            --test-max-frames=120
            --test-rounds=5
            --test-instances-per-round=256
    )

    set_tests_properties(Smoke.ResizeStressRuntime Smoke.DescriptorStressRuntime PROPERTIES
            LABELS "smoke;runtime"
            WORKING_DIRECTORY $<TARGET_FILE_DIR:App-Tumbler>
            TIMEOUT 60
    )

    if(WIN32)
        if(NOT DEFINED _vcpkg_triplet)
            set(_vcpkg_triplet "${VCPKG_TARGET_TRIPLET}")
            if(NOT _vcpkg_triplet)
                set(_vcpkg_triplet "x64-windows")
            endif()
            if(_vcpkg_triplet MATCHES "^\\$env:")
                set(_vcpkg_triplet "x64-windows")
            endif()
        endif()

        set(_runtime_smoke_env_mod_list
                "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/vcpkg_installed/${_vcpkg_triplet}/debug/bin"
                "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/vcpkg_installed/${_vcpkg_triplet}/bin"
        )
        list(JOIN _runtime_smoke_env_mod_list ";" _runtime_smoke_env_mod)

        set_tests_properties(Smoke.ResizeStressRuntime Smoke.DescriptorStressRuntime PROPERTIES
                ENVIRONMENT_MODIFICATION "${_runtime_smoke_env_mod}"
        )
    endif()
endif()
