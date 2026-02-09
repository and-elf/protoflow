# Unix socket transport library tests

add_executable(test_transport_unix
    test_unix.cpp
)

target_link_libraries(test_transport_unix
    PRIVATE
        protoflow::transport-unix
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_transport_unix)
