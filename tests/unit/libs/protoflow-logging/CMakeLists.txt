# protoflow-logging unit tests

add_executable(test_logging
    test_logging.cpp
)

target_link_libraries(test_logging
    PRIVATE
        protoflow::logging
        protoflow::service
        protoflow::messaging
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_logging)
