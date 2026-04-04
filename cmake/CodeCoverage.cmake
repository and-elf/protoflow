# Code coverage support using gcov/lcov

if(PROTOFLOW_ENABLE_COVERAGE)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(WARNING "Code coverage works best with Debug build type")
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
        
        message(STATUS "Code coverage enabled")
        
        # Add coverage target
        find_program(LCOV lcov)
        find_program(GENHTML genhtml)
        
        if(LCOV AND GENHTML)
            add_custom_target(coverage
                COMMAND ${LCOV} --directory . --capture --output-file coverage.info
                COMMAND ${LCOV} --remove coverage.info '/usr/*' '*/tests/*' '*/googletest/*' --output-file coverage.info
                COMMAND ${GENHTML} coverage.info --output-directory coverage
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generating code coverage report"
            )
            
            message(STATUS "Coverage report target added: make coverage")
        else()
            message(STATUS "lcov/genhtml not found - coverage target not available")
        endif()
    else()
        message(WARNING "Code coverage only supported for GCC and Clang")
    endif()
endif()
