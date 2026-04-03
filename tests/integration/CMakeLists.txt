# Integration tests (disabled - logging library has compilation errors)
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
#         GTest::gtest
#         GTest::gtest_main
# )
# 
# gtest_discover_tests(test_http_service)

# Main app runtime integration tests (disabled - missing protoflow::config)
# add_executable(test_main_app_runtime
#     test_main_app_runtime.cpp
#     ../../apps/main-app/src/runtime.cpp
#     ../../apps/main-app/src/services/app_registration_service.cpp
#     ../../apps/main-app/src/services/hardware_arbitration_service.cpp
#     ../../apps/main-app/src/services/http_service.cpp
# )
# 
# target_include_directories(test_main_app_runtime
#     PRIVATE
#         ${CMAKE_SOURCE_DIR}/apps/main-app/include
# )
# 
# target_link_libraries(test_main_app_runtime
#     PRIVATE
#         protoflow::service
#         protoflow::messaging
#         protoflow::logging
#         protoflow::runtime
#         protoflow::http
#         protoflow::html-fragment
#         protoflow::config
#         protoflow::hw-client
#         GTest::gtest
#         GTest::gtest_main
# )
# 
# gtest_discover_tests(test_main_app_runtime)

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
