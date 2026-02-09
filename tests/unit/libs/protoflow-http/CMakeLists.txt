# Unit tests for protoflow-http

add_executable(test_http
    test_request_response.cpp
    test_json.cpp
    test_router.cpp
    test_service.cpp
)

target_link_libraries(test_http
    PRIVATE
        protoflow::http
        GTest::gtest_main
)

gtest_discover_tests(test_http)
