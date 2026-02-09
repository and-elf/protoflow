# Static analyzer integration (clang-tidy, cppcheck)

option(ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
option(ENABLE_CPPCHECK "Enable cppcheck" OFF)

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY clang-tidy)
    if(CLANG_TIDY)
        set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY};
            -checks=*,-fuchsia-*,-google-*,-zircon-*,-abseil-*,-modernize-use-trailing-return-type;
            -warnings-as-errors=*;
        )
        message(STATUS "clang-tidy enabled")
    else()
        message(WARNING "clang-tidy requested but not found")
    endif()
endif()

if(ENABLE_CPPCHECK)
    find_program(CPPCHECK cppcheck)
    if(CPPCHECK)
        set(CMAKE_CXX_CPPCHECK ${CPPCHECK};
            --enable=all;
            --suppress=missingInclude;
            --inline-suppr;
            --inconclusive;
        )
        message(STATUS "cppcheck enabled")
    else()
        message(WARNING "cppcheck requested but not found")
    endif()
endif()
