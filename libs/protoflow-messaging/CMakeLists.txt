# protoflow-messaging library
# Message passing infrastructure

protoflow_add_library(
    NAME messaging
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Message passing infrastructure"
    TYPE STATIC
    
    SOURCES
        src/message.cpp
    
    PUBLIC_INCLUDE_DIRS
        include
)
