# protoflow-rpc-service

RPC Service - manages multiple network connections via message passing.

## Overview

This library provides a service that handles all RPC/network communication through messages, allowing other services to perform I/O without directly managing connections.

## Features

- **Message-based I/O**: All communication via service messages
- **Connection pooling**: Manages multiple simultaneous connections
- **Asynchronous operations**: Non-blocking send/receive
- **Error handling**: Reports errors via messages
- **Transport abstraction**: Currently uses TCP, can be extended

## Usage

### Basic Example

```cpp
#include <protoflow/rpc_service.hpp>
#include <protoflow/runtime.hpp>

using namespace protoflow;

// Create RPC service
auto rpc_svc = std::make_shared<rpc_service::RpcService>();
runtime.add_service(rpc_svc);

// Request connection from your service
ConnectionId conn_id = 1;
write(rpc_service::RpcConnectRequest{
    .connection_id = conn_id,
    .host = "localhost",
    .port = 8080
});

// Handle connection response
void handle(Message&& msg) {
    if (auto* connected = std::get_if<rpc_service::RpcConnected>(&msg)) {
        // Connection established, send data
        write(rpc_service::RpcSendRequest{
            .connection_id = connected->connection_id,
            .data = my_data
        });
    }
    else if (auto* received = std::get_if<rpc_service::RpcReceived>(&msg)) {
        // Data received
        process_data(received->data);
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
