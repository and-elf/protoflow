# Main-app unit tests

add_executable(test_main-app
    test_app_registration_service.cpp
    test_app_registration_tcp_integration.cpp
    test_http_service_static_files.cpp
    ${CMAKE_SOURCE_DIR}/tests/helpers/test_runtime.cpp
    ${CMAKE_SOURCE_DIR}/apps/main-app/src/services/app_registration_service.cpp
    ${CMAKE_SOURCE_DIR}/apps/main-app/src/services/rpc_server_service.cpp
    ${CMAKE_SOURCE_DIR}/apps/main-app/src/services/http_service.cpp
    ${CMAKE_SOURCE_DIR}/apps/main-app/src/services/handlers/static_handler.cpp
    ${CMAKE_SOURCE_DIR}/apps/main-app/src/services/handlers/home_handler.cpp
    ${CMAKE_SOURCE_DIR}/apps/main-app/src/services/handlers/status_handler.cpp
    ${CMAKE_SOURCE_DIR}/apps/main-app/src/services/handlers/state_handler.cpp
    ${CMAKE_SOURCE_DIR}/apps/main-app/src/services/handlers/app_handler.cpp
)

target_include_directories(test_main-app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/apps/main-app/include
)

target_link_libraries(test_main-app
    PRIVATE
        protoflow::service
        protoflow::fsm-table
        protoflow::logging
        protoflow::messaging
        protoflow::app-registration-protocol
        protoflow::app-registration-client
        protoflow::transport-tcp
        protoflow::rpc-service
        protoflow::http
        protoflow::html-fragment
        nlohmann_json::nlohmann_json
        GTest::gtest
        GTest::gtest_main
)

add_test(NAME test_main-app COMMAND test_main-app)
