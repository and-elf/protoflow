# protoflow-runtime library
# Core runtime and application base

protoflow_add_library(
    NAME runtime
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Core runtime and application base"
    TYPE STATIC
    
    SOURCES
        src/app_base.cpp
    
    PUBLIC_INCLUDE_DIRS
        include
    
    PUBLIC_LINK_LIBRARIES
        protoflow::messaging
        protoflow::service
)
