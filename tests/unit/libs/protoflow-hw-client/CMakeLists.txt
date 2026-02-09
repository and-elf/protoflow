add_executable(test_hw_client
    test_hw_client.cpp
    test_protocol.cpp
)

target_link_libraries(test_hw_client
    PRIVATE
        protoflow::hw-client
        protoflow::rpc
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_hw_client)
