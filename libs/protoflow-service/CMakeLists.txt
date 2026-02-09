# protoflow-service library
# Service base class and mailbox infrastructure

protoflow_add_library(
    NAME service
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Service base class and mailbox infrastructure"
    TYPE STATIC
    
    SOURCES
        # To be added
        # src/service.cpp
        # src/mailbox.cpp
    
    PUBLIC_INCLUDE_DIRS
        include
    
    PUBLIC_LINK_LIBRARIES
        protoflow::runtime
        protoflow::messaging
)
