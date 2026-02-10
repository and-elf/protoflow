# App Registration Client library tests

add_executable(test_app_registration_client
    test_app_registration_client_basic.cpp
    test_app_registration_client_fsm.cpp
    test_app_registration_client_protocol.cpp
)

target_link_libraries(test_app_registration_client
    PRIVATE
        protoflow::app-registration-client
        protoflow::app-registration-protocol
        protoflow::fsm
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
)

# Add test-specific include path for logging stub
target_include_directories(test_app_registration_client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Define macro to use test logging stub instead of real logging
target_compile_definitions(test_app_registration_client
    PRIVATE
        PROTOFLOW_TEST_MODE=1
)

gtest_discover_tests(test_app_registration_client)
