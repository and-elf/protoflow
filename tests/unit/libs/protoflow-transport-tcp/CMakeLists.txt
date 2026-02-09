# TCP transport library tests

add_executable(test_transport_tcp
    test_tcp.cpp
)

target_link_libraries(test_transport_tcp
    PRIVATE
        protoflow::transport-tcp
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_transport_tcp)
