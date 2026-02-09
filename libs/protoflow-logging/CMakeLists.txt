# protoflow-logging library
# Policy-injected logging infrastructure

protoflow_add_library(
    NAME logging
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Policy-injected logging infrastructure"
    TYPE STATIC
    
    SOURCES
        # To be added
        # src/logger.cpp
        # src/sinks.cpp
        # src/config_parser.cpp
    
    PUBLIC_INCLUDE_DIRS
        include
)
