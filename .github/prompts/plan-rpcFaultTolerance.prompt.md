# Plan: RPC Fault Tolerance Refactor

**TL;DR**: Replace busy polling with event-based synchronization + timeouts, add connection retry logic (exponential backoff, configurable), implement explicit heartbeats for connection health, handle lifecycle state transitions with acknowledgments, add graceful degradation when RPC server fails, and prevent resource leaks on unexpected disconnects. The refactor targets 5 core areas: synchronization, retry/reconnection, resource lifecycle, connection monitoring, and graceful degradation. This will fix the failing integration tests where skeleton apps crash due to unhandled connection failures.

## Implementation Steps

### 1. Add Connection State Management & Retry Logic to RPC Client Service

**Files to modify:**
- [libs/protoflow-rpc-service/include/protoflow/rpc_service/rpc_client_service.hpp](libs/protoflow-rpc-service/include/protoflow/rpc_service/rpc_client_service.hpp)
- [libs/protoflow-rpc-service/src/rpc_client_service.cpp](libs/protoflow-rpc-service/src/rpc_client_service.cpp)

**Changes to header:**
- Add `ConnectionState` enum: `DISCONNECTED`, `CONNECTING`, `CONNECTED`, `ERROR`, `BACKOFF`
- Add `RetryConfig` struct with: `max_retries`, `backoff_multiplier`, `initial_delay_ms`, `max_delay_ms`
- Add state tracking variables and backoff timer
- Add `connect_with_retry()` method signature
- Add `handle_connection_error()` method signature
- Add `get_connection_state()` const method
- Add `get_last_activity_time()` const method

**Changes to implementation:**
- Replace busy polling in `poll_connection()` with event-based approach: use transport's condition variable notifications when available
- Implement `connect_with_retry()` method with exponential backoff logic
- Implement `handle_connection_error()` to transition to `BACKOFF` state instead of crashing
- Implement timeout-based fallback polling if transport doesn't support events
- Track last activity timestamp for timeout detection
- Update `poll()` to only attempt actions appropriate to current state

### 2. Replace Busy Polling with Event-Based Synchronization

**Files to modify:**
- [libs/protoflow-rpc-service/src/rpc_client_service.cpp](libs/protoflow-rpc-service/src/rpc_client_service.cpp)
- [libs/protoflow-rpc-service/include/protoflow/rpc_service/rpc_client_service.hpp](libs/protoflow-rpc-service/include/protoflow/rpc_service/rpc_client_service.hpp)

**Changes:**
- Modify `poll()` to:
  - Check for event notifications first (non-blocking)
  - Use configurable timeout for blocking wait on condition variable
  - Track state transitions and only attempt actions appropriate to current state
  - Fall back to timeout-based polling (10ms) if events unavailable
- Expose `get_connection_state()` method in header
- Expose `get_last_activity_time()` method in header
- Add member variables for event-based synchronization (condition variable, state lock)

### 3. Add Explicit Heartbeat Mechanism

**Files to modify:**
- [libs/protoflow-rpc-service/src/rpc_client_service.cpp](libs/protoflow-rpc-service/src/rpc_client_service.cpp)
- [libs/protoflow-rpc-service/include/protoflow/rpc_service/rpc_client_service.hpp](libs/protoflow-rpc-service/include/protoflow/rpc_service/rpc_client_service.hpp)
- [libs/protoflow-rpc-protocol/include/protoflow/rpc/protocol.hpp](libs/protoflow-rpc-protocol/include/protoflow/rpc/protocol.hpp) (or similar)
- [apps/main-app/src/services/rpc_server_service.cpp](apps/main-app/src/services/rpc_server_service.cpp)

**Changes to protocol:**
- Define new heartbeat message type (e.g., HEARTBEAT = 0xFFFF command, or separate frame type)

**Changes to RPC Client Service:**
- Add heartbeat timer and interval configuration
- Send heartbeat when idle for configurable period (e.g., 5s)
- Track heartbeat responses to detect unresponsive connections
- Trigger reconnection on heartbeat timeout
- Add heartbeat response handler

**Changes to RPC Server Service:**
- Respond to heartbeat messages
- Track per-client last activity timestamp
- Remove clients that haven't sent/received data within timeout window (e.g., 30s)

### 4. Fix App Registration Lifecycle & State Transitions

**Files to modify:**
- [libs/protoflow-app-registration-client/include/app_registration_client.hpp](libs/protoflow-app-registration-client/include/app_registration_client.hpp)
- [libs/protoflow-app-registration-client/src/app_registration_client.cpp](libs/protoflow-app-registration-client/src/app_registration_client.cpp)
- [apps/skeleton-app/src/app.cpp](apps/skeleton-app/src/app.cpp)

**Changes to AppRegistrationClient header:**
- Add `RegistrationState` enum: `IDLE`, `CONNECTING`, `REGISTERING`, `REGISTERED`, `DISCONNECTED`, `ERROR`
- Add `wait_for_registered(timeout_ms)` method to block until registration succeeds or times out
- Add error callback for handling registration failures
- Add `get_registration_state()` const method

**Changes to AppRegistrationClient implementation:**
- Handle connection failures without crashing FSM
- Add exponential backoff to registration attempts
- Transition to `ERROR` state instead of crashing; allow retry
- Implement `wait_for_registered()` using condition variable or polling with timeout
- Add proper exception handling in FSM transitions

**Changes to skeleton-app:**
- Use `wait_for_registered()` with timeout before proceeding with business logic
- Catch exceptions and retry registration instead of crashing
- Implement graceful shutdown if registration fails after max retries
- Log error states clearly for debugging

### 5. Add Timeout Detection & Graceful Cleanup in RPC Server

**Files to modify:**
- [apps/main-app/src/services/rpc_server_service.hpp](apps/main-app/src/services/rpc_server_service.hpp)
- [apps/main-app/src/services/rpc_server_service.cpp](apps/main-app/src/services/rpc_server_service.cpp)

**Changes to header:**
- Add `ClientConnection` struct/class with:
  - `connection_ptr` (transport instance)
  - `last_activity_time` (timestamp)
  - `heartbeat_timer` (for tracking next heartbeat)
  - `client_id` or connection identifier
- Add configurable timeout window member (e.g., 30s)
- Add getter methods for connection status and activity

**Changes to implementation:**
- Track per-client last activity timestamp
- In `poll()`, detect and remove clients exceeding inactivity timeout
- Send resource cleanup/deregistration notifications before removing dead clients
- Handle client disconnects gracefully (notify registration service of deregistration)
- Implement connection cleanup logic to prevent resource leaks
- Add logging for connection state transitions

### 6. Implement Resource Lifecycle Transitions

**Files to modify:**
- [apps/main-app/src/services/rpc_server_service.cpp](apps/main-app/src/services/rpc_server_service.cpp)
- [apps/main-app/include/services/rpc_server_service.hpp](apps/main-app/include/services/rpc_server_service.hpp)

**Changes:**
- Add `on_client_registered()` callback to validate resources are available
- Add `on_client_disconnected()` callback to clean up resources
- Use these callbacks to notify dependent services (e.g., app registration service)
- Ensure deregistration messages are sent to app registration service on unexpected disconnect
- Add state tracking for resource allocation/deallocation per client
- Prevent duplicate registrations for same app

### 7. Add Graceful Degradation to Main App

**Files to modify:**
- [apps/main-app/src/app.cpp](apps/main-app/src/app.cpp)
- [apps/main-app/include/app.hpp](apps/main-app/include/app.hpp)

**Changes:**
- Allow RPC server initialization to fail without crashing main app
- Track RPC server health status as a state variable
- Add `/health` endpoint that reports RPC server status (OK/DEGRADED/ERROR)
- Queue app registrations if RPC not yet ready; replay when RPC recovers
- Update main app startup to log warnings instead of fatal errors on RPC init failure
- Add retry logic for RPC server initialization in main app poll loop
- Gracefully handle missing RPC server in message routing

### 8. Update Integration Tests & Add New Ones

**Files to modify/create:**
- [tests/integration/test_client_server.cpp](tests/integration/test_client_server.cpp)
- New test files for new scenarios

**Changes to existing tests:**
- Update timeouts to allow for retry backoff (increase from 3s to 10-15s for connection establishment)
- Add assertions for state transitions
- Add assertions for successful reconnection after failures
- Verify state transitions in logs
- Add diagnostic logging to help debug state machine issues

**New tests to add:**
- `ClientServerIntegration.ClientReconnectsAfterServerRestart` - Kill server, restart it, verify client reconnects
- `ClientServerIntegration.HeartbeatExchange` - Verify heartbeat messages are exchanged
- `ClientServerIntegration.ServerRemovesInactiveClients` - Verify timeout-based client removal
- `ClientServerIntegration.ClientGracesfullyDegrades` - Verify app continues if registration fails
- `ConnectionRecovery.ExponentialBackoffRetry` - Verify backoff behavior with configurable delays
- `ResourceLifecycle.CleanupOnUnexpectedDisconnect` - Verify resources cleaned up when client drops

## Verification Checklist

### Build & Compilation
- [ ] `cmake --build build -j $(nproc)` completes successfully
- [ ] All new headers compile without errors
- [ ] No new compiler warnings introduced

### Integration Tests
- [ ] `ctest --output-on-failure` from `build/` directory passes
- [ ] `ClientServerIntegration.SkeletonAppConnectsToMainApp` passes
- [ ] `ClientServerIntegration.MultipleClientsConnect` passes
- [ ] `ClientServerIntegration.ClientSurvivesServerRestart` passes
- [ ] `RegistrationStatus.StatusEndpointShowsRegisteredApps` passes
- [ ] All new tests pass

### Manual Testing
- [ ] Start main app, start skeleton app, kill main app → skeleton app reconnects
- [ ] Kill skeleton app → main app detects and cleans up resources (verify logs)
- [ ] Check `/health` endpoint with RPC server running → reports OK
- [ ] Check `/health` endpoint with RPC server failed → reports DEGRADED
- [ ] Monitor process memory over extended runs with repeated connect/disconnect cycles

### Logging Verification
- [ ] Connection state transitions logged
- [ ] Retry attempts logged with backoff delay
- [ ] Heartbeat exchanges logged (at DEBUG level to avoid spam)
- [ ] Client timeout/removal events logged
- [ ] Resource cleanup events logged

## Design Decisions

1. **Sync Model**: Event-based with configurable timeout fallback
   - Preferred: Use transport's condition variable notifications when available
   - Fallback: Timeout-based polling (10ms) if events unavailable
   - Rationale: Balances efficiency with compatibility

2. **Retry Strategy**: Exponential backoff configurable per-connection
   - Formula: `delay = min(initial_delay * (multiplier ^ attempt), max_delay)`
   - Randomization: Add jitter (±10%) to prevent thundering herd
   - Rationale: Allows different strategies for different endpoints

3. **Heartbeat**: Explicit periodic heartbeat messages
   - Interval: 5s for sending, 30s timeout for detection
   - Message: Lightweight PING/PONG protocol
   - Rationale: Detects dead connections quickly and reliably

4. **Degradation**: Main app continues running if RPC server fails
   - Behavior: Log warnings, attempt restart in background
   - HTTP endpoints: Return 503 Service Unavailable with retry info
   - Rationale: Allows partial operation and detection of failures

5. **Backward Compatibility**: Keep existing polling-based API functional
   - Optimize for event-aware transports
   - Fall back to polling for legacy transports
   - Rationale: No API breakage, incremental upgrade path

## Implementation Order

1. **Phase 1 - Foundation** (enables other phases):
   - Step 1: Connection state management
   - Step 2: Event-based synchronization

2. **Phase 2 - Resilience**:
   - Step 3: Heartbeat mechanism
   - Step 4: App registration lifecycle fixes

3. **Phase 3 - Server-side robustness**:
   - Step 5: Timeout detection & cleanup
   - Step 6: Resource lifecycle transitions

4. **Phase 4 - System-wide integration**:
   - Step 7: Graceful degradation
   - Step 8: Test updates & verification

## Risk Mitigation

- **Risk**: Breaking existing behavior with state machine changes
  - **Mitigation**: Add comprehensive unit tests for state transitions before integration tests
  - **Rollback**: Keep old polling code as fallback until new code proven stable

- **Risk**: Deadlocks from condition variables
  - **Mitigation**: Use RAII lock guards, avoid nested locks, set timeouts on all waits

- **Risk**: Memory leaks from incomplete client cleanup
  - **Mitigation**: Use unique_ptr for client connections, test with valgrind/asan

- **Risk**: Lost app registrations during server restart
  - **Mitigation**: Implement deregistration on disconnect, persist registration state if needed

- **Risk**: Increased CPU usage from heartbeat logic
  - **Mitigation**: Use configurable heartbeat intervals, start high (e.g., 30s) and tune based on testing
