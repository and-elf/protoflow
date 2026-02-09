# protoflow-rpc library
# RPC framework for inter-app communication

protoflow_add_library(
    NAME rpc
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "RPC framework for inter-app communication"
    TYPE STATIC
    
    SOURCES
        # To be added
        # src/rpc_server_base.cpp
        # src/rpc_client_base.cpp
        # src/protocol.cpp
    
    PUBLIC_INCLUDE_DIRS
        include
    
    PUBLIC_LINK_LIBRARIES
        protoflow::runtime
        protoflow::messaging
)
