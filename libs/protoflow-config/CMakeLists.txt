# protoflow-config library
# Configuration file parsers (INI, hardware config, logging config)

protoflow_add_library(
    NAME config
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Configuration file parsers for protoflow"
    TYPE STATIC
    
    SOURCES
        src/ini_parser.cpp
        src/hardware_config.cpp
        src/logging_config.cpp
        src/config_paths.cpp
    
    PUBLIC_INCLUDE_DIRS
        include
    
    PUBLIC_LINK_LIBRARIES
        protoflow::messaging
        protoflow::logging
)
