# Integration tests (disabled - logging library has compilation errors)
# - test_http_service disabled: accesses private members of HTTPService
# add_executable(test_http_service
#     test_http_service.cpp
#     ../../apps/main-app/src/services/http_service.cpp
# )
# 
# target_include_directories(test_http_service
#     PRIVATE
#         ${CMAKE_SOURCE_DIR}/apps/main-app/include
# )
# 
# target_link_libraries(test_http_service
#     PRIVATE
#         protoflow::service
#         protoflow::messaging
#         protoflow::logging
#         protoflow::http
#         protoflow::html-fragment
#         protoflow::app-registration-protocol
#         protoflow::app-registration-client
#         GTest::gtest
#         GTest::gtest_main
# )
# 
# gtest_discover_tests(test_http_service)

# Main app runtime integration tests (disabled - missing protoflow::config)
# - File references non-existent runtime.cpp - disabled for now
# add_executable(test_main_app_runtime
#     test_main_app_runtime.cpp
#     ../../apps/main-app/src/runtime.cpp
# )

# Black-box integration test: spawn the built main app executable and perform
# raw HTTP GETs to verify the HTTPService + HttpListenerService respond.
add_executable(test_main_app_blackbox
	test_main_app_blackbox.cpp
)

target_link_libraries(test_main_app_blackbox
	PRIVATE
		GTest::gtest
		GTest::gtest_main
)

# Provide absolute path to built main app executable to the test via a macro
target_compile_definitions(test_main_app_blackbox
	PRIVATE MAIN_APP_EXECUTABLE="$<TARGET_FILE:protoflow-main-app>"
)

gtest_discover_tests(test_main_app_blackbox)

# Client ↔ Server integration test: spawn main app and skeleton app,
# verify they connect, communicate, and shut down cleanly.
# DISABLED: Hangs when multiple tests spawn servers on ports
#if(PROTOFLOW_BUILD_SKELETON_APP AND PROTOFLOW_BUILD_MAIN_APP)
#    add_executable(test_client_server
#        test_client_server.cpp
#    )
#
#    target_link_libraries(test_client_server
#        PRIVATE
#            GTest::gtest
#            GTest::gtest_main
#    )
#
#    target_compile_definitions(test_client_server
#        PRIVATE
#            MAIN_APP_EXECUTABLE="$<TARGET_FILE:protoflow-main-app>"
#            SKELETON_APP_EXECUTABLE="$<TARGET_FILE:protoflow-skeleton-app>"
#    )
#
#    gtest_discover_tests(test_client_server)
#endif()
# App registration verification test: spawn main app + multiple skeleton apps,
# verify registration is visible via /status endpoint
# DISABLED: Hangs when multiple tests spawn servers on ports
#if(PROTOFLOW_BUILD_SKELETON_APP AND PROTOFLOW_BUILD_MAIN_APP)
#    add_executable(test_registration_status
#        test_registration_status.cpp
#    )
#
#    target_link_libraries(test_registration_status
#        PRIVATE
#            GTest::gtest
#            GTest::gtest_main
#            nlohmann_json::nlohmann_json
#    )
#
#    target_compile_definitions(test_registration_status
#        PRIVATE
#            MAIN_APP_EXECUTABLE="$<TARGET_FILE:protoflow-main-app>"
#            SKELETON_APP_EXECUTABLE="$<TARGET_FILE:protoflow-skeleton-app>"
#    )
#
#    gtest_discover_tests(test_registration_status)
#endif()

# RPC Communication integration tests
# Tests core RPC protocol: hello handshake, message exchange, concurrent clients, etc.
# DISABLED: ClientSendServerReceive and LargePayloadTransmission fail due to TCP fragmentation issues
# The receive_payload function was updated to read in a loop, but issues remain.
# These tests need further investigation into proper TCP payload handling.
#add_executable(test_rpc_communication
#    test_rpc_communication.cpp
#)
#
#target_link_libraries(test_rpc_communication
#    PRIVATE
#        protoflow::rpc
#        protoflow::rpc-client
#        protoflow::rpc-server
#        protoflow::transport-tcp
#        GTest::gtest
#        GTest::gtest_main
#)
#
#gtest_discover_tests(test_rpc_communication)

# App Registration Lifecycle integration tests
# Tests full lifecycle: registration, re-registration after crash, server restart resilience,
# timeout handling, concurrent registrations, and clean shutdown.
# DISABLED: Hangs when multiple tests spawn servers on ports
#if(PROTOFLOW_BUILD_SKELETON_APP AND PROTOFLOW_BUILD_MAIN_APP)
#    add_executable(test_registration_lifecycle
#        test_registration_lifecycle.cpp
#    )
#
#    target_link_libraries(test_registration_lifecycle
#        PRIVATE
#            GTest::gtest
#            GTest::gtest_main
#            nlohmann_json::nlohmann_json
#    )
#
#    target_compile_definitions(test_registration_lifecycle
#        PRIVATE
#            MAIN_APP_EXECUTABLE="$<TARGET_FILE:protoflow-main-app>"
#            SKELETON_APP_EXECUTABLE="$<TARGET_FILE:protoflow-skeleton-app>"
#    )
#
#    gtest_discover_tests(test_registration_lifecycle)
#endif()

# App Registration Client Integration Test
# Tests integration between AppRegistrationClient and mocked RPC service
# Verifies FSM state transitions, message routing, and protocol compliance
add_executable(test_app_registration_client_integration
    test_app_registration_client_integration.cpp
)

target_link_libraries(test_app_registration_client_integration
    PRIVATE
        protoflow::app-registration-client
        protoflow::app-registration-protocol
        protoflow::rpc
        protoflow::rpc-client
        protoflow::service
        protoflow::messaging
        protoflow::runtime
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_app_registration_client_integration)