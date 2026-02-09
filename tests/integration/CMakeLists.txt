# Integration tests

add_executable(test_http_service
    test_http_service.cpp
    ../../apps/main-app/src/services/http_service.cpp
)

target_include_directories(test_http_service
    PRIVATE
        ${CMAKE_SOURCE_DIR}/apps/main-app/include
)

target_link_libraries(test_http_service
    PRIVATE
        protoflow::service
        protoflow::messaging
        protoflow::logging
        protoflow::http
        protoflow::html-fragment
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_http_service)
