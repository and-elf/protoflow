# Protoflow Logging Library

Documentation: [libs/protoflow-logging/README.md](../libs/protoflow-logging/README.md)

## Quick Reference

### Including the Library

```cpp
#include <protoflow/logging/logging.hpp>
```

### Logging in Services

```cpp
// Direct method calls
log_trace("Trace message");
log_debug("Debug message");
log_info("Info message");
log_warn("Warning message");
log_error("Error message");
log_fatal("Fatal message");

// Stream-style with macros
PROTOFLOW_LOG_INFO(*this, "Value: " << value);
```

### LoggingService Configuration

```cpp
auto logger = std::make_shared<protoflow::logging::LoggingService>();
logger->set_min_level(protoflow::logging::Level::Debug);
logger->set_console_output(true);
logger->set_max_stored_logs(1000);
```

## See Also

- [library-service.md](library-service.md) - Service base class
- [library-rpc.md](library-rpc.md) - RPC implementation
