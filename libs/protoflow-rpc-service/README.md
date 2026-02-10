# protoflow-rpc-service

RPC Client Service - manages multiple outbound network connections via message passing.

## Overview

This library provides a **client-side** service that handles outbound RPC/network connections through messages, allowing other services to perform I/O without directly managing connections.

**Important**: This is the CLIENT service for making outbound connections. Each system should have ONE RPC SERVER (implemented in main-app) and can have multiple instances of this client service if needed.

## Architecture

- **RpcClientService** (this library) - Makes outbound connections to remote servers
- **RpcServerService** (in main-app) - Accepts inbound connections (ONE per installation)

## Features

- **Message-based I/O**: All communication via service messages
- **Connection pooling**: Manages multiple simultaneous outbound connections
- **Asynchronous operations**: Non-blocking send/receive
- **Error handling**: Reports errors via messages
- **Transport abstraction**: Currently uses TCP, can be extended

## Usage

### Basic Client Example

```cpp
#include <protoflow/rpc_service.hpp>
#include <protoflow/runtime.hpp>

using namespace protoflow;

// Create RPC client service
auto rpc_client = std::make_shared<rpc_service::RpcClientService>();
runtime.add_service(rpc_client);

// Request connection from your service
ConnectionId conn_id = 1;
write(rpc_service::RpcConnectRequest{
    .connection_id = conn_id,
    .host = "remote-server.example.com",
    .port = 8080
});

// Handle connection response
void handle(Message&& msg) {
    if (msg.is_type(rpc_service::RpcMessageTypes::Connected)) {
        auto connected = rpc_service::RpcConnected::deserialize(msg.data);
        // Connection established, send data
        write(rpc_service::RpcSendRequest{
            .connection_id = connected.connection_id,
            .data = my_data
        });
    }
    else if (msg.is_type(rpc_service::RpcMessageTypes::Received)) {
        auto received = rpc_service::RpcReceived::deserialize(msg.data);
        // Data received from server
        process_data(received.data);
    }
}
```
    }
}
```

## Message Types

### Requests (sent to RpcService)

- `RpcConnectRequest` - Establish connection
- `RpcSendRequest` - Send data
- `RpcDisconnectRequest` - Close connection

### Responses (received from RpcService)

- `RpcConnected` - Connection established
- `RpcConnectionFailed` - Connection failed
- `RpcSent` - Data sent successfully
- `RpcSendFailed` - Send failed
- `RpcReceived` - Data received
- `RpcDisconnected` - Connection closed
- `RpcError` - Error occurred

## Dependencies

- `protoflow::service` - Base service class
- `protoflow::transport-tcp` - TCP transport
- `protoflow::logging` - Logging infrastructure
