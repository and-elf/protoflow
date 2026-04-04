# protoflow-config unit tests

add_executable(test_config
    test_config_paths.cpp
)

target_link_libraries(test_config
    PRIVATE
        protoflow::config
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_config)
