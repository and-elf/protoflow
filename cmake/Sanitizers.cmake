# Sanitizer support

function(enable_sanitizers)
    set(SANITIZER_FLAGS "")

    if(PROTOFLOW_ENABLE_SANITIZER_ADDRESS)
        list(APPEND SANITIZER_FLAGS "-fsanitize=address")
        message(STATUS "Address Sanitizer enabled")
    endif()

    if(PROTOFLOW_ENABLE_SANITIZER_UB)
        list(APPEND SANITIZER_FLAGS "-fsanitize=undefined")
        message(STATUS "Undefined Behavior Sanitizer enabled")
    endif()

    if(PROTOFLOW_ENABLE_SANITIZER_THREAD)
        if(PROTOFLOW_ENABLE_SANITIZER_ADDRESS)
            message(FATAL_ERROR "Cannot enable both Address and Thread sanitizers")
        endif()
        list(APPEND SANITIZER_FLAGS "-fsanitize=thread")
        message(STATUS "Thread Sanitizer enabled")
    endif()

    if(SANITIZER_FLAGS)
        add_compile_options(${SANITIZER_FLAGS})
        add_link_options(${SANITIZER_FLAGS})
    endif()
endfunction()
