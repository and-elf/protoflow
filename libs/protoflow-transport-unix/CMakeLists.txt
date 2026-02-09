# protoflow-transport-unix library
# Unix socket transport adapter

protoflow_add_library(
    NAME transport-unix
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Unix socket transport adapter"
    TYPE STATIC
    
    SOURCES
        src/unix.cpp
    
    PUBLIC_INCLUDE_DIRS
        include
)
