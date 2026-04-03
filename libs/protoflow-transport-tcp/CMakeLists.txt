# protoflow-transport-tcp library
# TCP transport adapter

protoflow_add_library(
    NAME transport-tcp
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "TCP transport adapter"
    TYPE STATIC
    
    SOURCES
        src/tcp.cpp
    
    PUBLIC_INCLUDE_DIRS
        include
)

target_link_libraries(protoflow-transport-tcp
    PUBLIC
        protoflow-rpc-protocol
)
