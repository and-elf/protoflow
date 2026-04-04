# Protoflow Skeleton App

A template/skeleton client application for the protoflow framework.

## Purpose

This app serves as:
- A **reference implementation** showing how to use protoflow libraries
- A **starting point** for new client applications (copy and modify)
- An **integration test client** that exercises server+client together

## Features Demonstrated

| Feature | Library | Description |
|---------|---------|-------------|
| App Registration | `protoflow::app-registration-client` | FSM-driven connection to main app |
| Hardware Access | `protoflow::hw-client` | Request/release device access via RPC |
| RPC Client | `protoflow::rpc-client` | Custom command handling |
| Service Framework | `protoflow::service` | Message-driven service lifecycle |
| Messaging | `protoflow::messaging` | Inter-service message routing |
| Logging | `protoflow::logging` | Structured log messages |
| HTML Fragments | `protoflow::html-fragment` | Render HTML status fragments |
| Configuration | `protoflow::config` | INI-based configuration |

## Usage

```bash
# Basic: connect to main app on default address/port
protoflow-skeleton-app

# Custom server address
protoflow-skeleton-app -s 192.168.1.100 -p 9123

# With hardware access
protoflow-skeleton-app --enable-hw --hw-resources /dev/ttyUSB0

# Custom app name and endpoints
protoflow-skeleton-app -n my-sensor-app -e /sensor -e /sensor/data
```

## Creating a New App

1. Copy the `skeleton-app` directory
2. Rename files and namespaces
3. Add your custom services and logic
4. Update `CMakeLists.txt` with your dependencies
5. Add `add_subdirectory(apps/your-app)` to the top-level `CMakeLists.txt`
