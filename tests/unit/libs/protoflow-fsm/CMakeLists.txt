# FSM library tests

add_executable(test_fsm
    test_fsm_basic.cpp
    test_app_registration.cpp
    test_sinks.cpp
)

target_link_libraries(test_fsm
    PRIVATE
        protoflow::fsm
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_fsm)
