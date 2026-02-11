# RPC library tests

add_executable(test_rpc
    test_protocol.cpp
    test_rpc_base.cpp
    test_rpc_app.cpp
)

target_link_libraries(test_rpc
    PRIVATE
        protoflow::rpc-protocol
        protoflow::rpc-client
        protoflow::rpc-server
        protoflow::rpc  # For rpc_base.hpp and rpc_app.hpp
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_rpc)
