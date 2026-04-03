add_executable(TumblerUnitTests
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
gtest_discover_tests(TumblerUnitTests
        DISCOVERY_TIMEOUT 60
        PROPERTIES LABELS "unit"
)
