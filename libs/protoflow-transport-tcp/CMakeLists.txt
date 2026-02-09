# protoflow-transport-tcp library
# TCP transport adapter

protoflow_add_library(
    NAME transport-tcp
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "TCP transport adapter"
    TYPE STATIC
    
    SOURCES
        # To be added
        # src/tcp_client.cpp
        # src/tcp_server.cpp
        # src/tcp_adapter.cpp
    
    PUBLIC_INCLUDE_DIRS
        include
    
    PUBLIC_LINK_LIBRARIES
        protoflow::runtime
        protoflow::messaging
)
