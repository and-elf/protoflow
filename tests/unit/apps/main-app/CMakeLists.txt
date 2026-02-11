# Main-app unit tests

add_executable(test_main-app
    test_app_registration_service.cpp
    ${CMAKE_SOURCE_DIR}/apps/main-app/src/services/app_registration_service.cpp
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
        nlohmann_json::nlohmann_json
        GTest::gtest
        GTest::gtest_main
)

add_test(NAME test_main-app COMMAND test_main-app)
