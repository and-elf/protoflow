add_executable(test_protoflow-fsm-table
    test_fsm_table_basic.cpp
)

target_link_libraries(test_protoflow-fsm-table
    PRIVATE
        protoflow::fsm-table
        GTest::gtest
        GTest::gtest_main
)

add_test(NAME test_protoflow-fsm-table COMMAND test_protoflow-fsm-table)
