# Messaging library tests

add_executable(test_messaging
    test_message.cpp
    test_router.cpp
)

target_link_libraries(test_messaging
    PRIVATE
        protoflow::messaging
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_messaging)
