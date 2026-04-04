# protoflow-runtime library
# Core runtime, message router, and scheduler

protoflow_add_library(
    NAME runtime
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Core runtime, message router, and scheduler"
    TYPE STATIC
    
    SOURCES
        src/runtime.cpp
        src/scheduler.cpp
        src/app_base.cpp
    
    PUBLIC_INCLUDE_DIRS
        include
    
    PUBLIC_LINK_LIBRARIES
        protoflow::messaging
        protoflow::service
)
