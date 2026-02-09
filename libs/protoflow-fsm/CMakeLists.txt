# protoflow-fsm library
# Compile-time validated finite state machines

protoflow_add_library(
    NAME fsm
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Compile-time validated finite state machines"
    TYPE INTERFACE
    
    PUBLIC_INCLUDE_DIRS
        include
)
