# protoflow-transport-mqtt library
# MQTT transport adapter

protoflow_add_library(
    NAME transport-mqtt
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "MQTT transport adapter"
    TYPE STATIC
    
    SOURCES
        # To be added
        # src/mqtt_client.cpp
        # src/mqtt_adapter.cpp
    
    PUBLIC_INCLUDE_DIRS
        include
    
    PUBLIC_LINK_LIBRARIES
        protoflow::runtime
        protoflow::messaging
)
