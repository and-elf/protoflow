cmake_minimum_required(VERSION 3.20)

# Controller example
add_executable(fsm-controller-example controller_example.cpp)
target_link_libraries(fsm-controller-example PRIVATE protoflow::fsm-table)
target_compile_features(fsm-controller-example PRIVATE cxx_std_20)

# Enable warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(fsm-controller-example PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(fsm-controller-example PRIVATE /W4)
endif()

# Class member example
add_executable(fsm-class-member-example class_member_example.cpp)
target_link_libraries(fsm-class-member-example PRIVATE protoflow::fsm-table)
target_compile_features(fsm-class-member-example PRIVATE cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(fsm-class-member-example PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(fsm-class-member-example PRIVATE /W4)
endif()
