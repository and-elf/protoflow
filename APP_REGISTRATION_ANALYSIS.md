# App Registration Verification & Issues

## Summary

**YES**, skeleton apps **DO try to register** on startup, but **the `/status` endpoint is NOT showing the registered apps**. This is a critical issue that needs to be fixed.

## Test Results

Created integration test: `test_registration_status.cpp` that verifies app registration via the `/status` endpoint.

### Test Failures
All three tests **FAIL** because the `/status` endpoint returns:
```json
{
  "registered_apps": [],
  "status": "running",
  "system": { ... }
}
```

Even though skeleton apps successfully connected and are trying to register.

## Architecture Analysis

### What Works ✅
1. **Skeleton apps DO initialize**, can observe in logs:
   ```
   Initializing test-skeleton-1...
     - AppRegistrationClient (server=127.0.0.1:29130)
     - Starting services...
   test-skeleton-1 initialized.
   ```

2. **Skeleton apps call `registration_client_->start()`** which attempts to connect and register via TCP/RPC to the main app

3. **Main app's AppRegistrationService** is listening for registration messages on port 9130 (or specified RPC port)

4. **Main app's HTTP status endpoints** exist and respond:
   - `/status` - returns JSON with registered_apps 
   - `/api/status` - async variant

### What's Broken ❌

**The registration data is NOT flowing from AppRegistrationService to HTTPService**

#### Message Flow Issue

```
┌─────────────────┐
│  Skeleton App   │
└────────┬────────┘
         │
         │ RPC Message: register_app
         │ (AppRegistrationEvent)
         ▼
┌─────────────────────────────┐
│  Main App RPC Server        │
└────────┬────────────────────┘
         │
         │ Forwards to AppRegistrationService
         │
         ▼
┌─────────────────────────────┐
│ AppRegistrationService      │
├─────────────────────────────┤
│ - Receives register_app msg │
│ - Updates registered_apps_  │
│ - NO OUTPUT EVENT           │ ◄─── BUG: Should broadcast event
└─────────────────────────────┘
         │
         │ (No event sent!)
         ▼
┌─────────────────────────────┐
│ HTTPService                 │
├─────────────────────────────┤
│ - Subscribes to HttpRequest │
│ - HAS handle_app_registration() │
│ - But NEVER receives event  │ ◄─── BUG: Doesn't subscribe
│ - registered_apps_ stays    │
│   empty!                    │
└─────────────────────────────┘
```

#### Root Cause

In `apps/main-app/src/services/app_registration_service.cpp`:

```cpp
std::vector<messaging::Message> AppRegistrationService::generate_outbound() {
    // No outbound messages generated - using direct method calls instead
    return {};
}
```

The AppRegistrationService **does not publish any events** to notify other services when apps register.

Meanwhile in `apps/main-app/include/services/http_service.hpp`:

```cpp
void handle_app_registration(const AppRegistrationEvent& event);
void handle_app_unregistration(const AppUnregistrationEvent& event);
```

These handler methods exist but are **never called** because HTTPService doesn't subscribe to these event types.

## Solution Approaches

### Option A: Service-to-Service Communication via Message Bus (Recommended)

**Pros:**
- Decoupled architecture
- Scalable to multiple subscribers
- Follows the protoflow message-driven pattern

**Cons:**
- Requires careful handling of message types and subscriptions

**Implementation:**
1. AppRegistrationService publishes AppRegistrationEvent messages in `generate_outbound()`
2. HTTPService subscribes to AppRegistrationEvent and AppUnregistrationEvent message types
3. HTTPService handles these events in its `handle()` method

### Option B: Direct Service Access

**Pros:**
- Simple and direct
- Lower latency

**Cons:**
- Tight coupling
- Violates service independence principle
- Protoflow architecture uses message-based communication

**Implementation:**
1. HTTPService gets a reference to AppRegistrationService
2. HTTPService queries AppRegistrationService directly for registered apps
3. HTTPService populates its own copy or queries on-demand

### Option C: Hybrid Approach

**Pros:**
- Best of both worlds
- HTTPService queries on-demand, still gets notified

**Cons:**
- More complex

**Implementation:**
1. AppRegistrationService publishes change events when apps register/unregister
2. HTTPService listens for these events (so it knows when to update)
3. status endpoint queries AppRegistrationService directly for current state

## Recommended Fix

**Implement Option A** - the message bus approach is architecturally correct for protoflow.

### Changes needed:

#### 1. Update AppRegistrationService to publish events

In `apps/main-app/src/services/app_registration_service.cpp`:

- Store a list of pending outbound AppRegistrationEvent and AppUnregistrationEvent messages
- In `register_app()`, append an AppRegistrationEvent to pending outbound list
- In `unregister_app()`, append an AppUnregistrationEvent to pending outbound list  
- Return these from `generate_outbound()` instead of empty vector

#### 2. Update HTTPService to subscribe to registration events

In `apps/main-app/include/services/http_service.hpp`:

- Add message type subscriptions in constructor (HTTPService::HTTPService) for:
  - AppRegistrationEvent
  - AppUnregistrationEvent

#### 3. Call registration handlers

In `apps/main-app/src/services/http_service.cpp`:

- In the `handle()` method, detect AppRegistrationEvent and AppUnregistrationEvent message types
- Call `handle_app_registration()` and `handle_app_unregistration()` methods

## Files Involved

- AppRegistrationService header - Needs to publish events
- HTTPService header - Needs to listen for registration events
- app_registration_service.cpp - Implementation
- http_service.cpp - Implementation

## Test Files

- test_registration_status.cpp - NEW integration test suite
  - Verifies `/status` endpoint shows registered apps
  - Tests single and multiple skeleton app registration
  - Tests both `/status` and `/api/status` endpoints

Run tests with:
```bash
/home/andreas/work/protoflow/build/tests/integration/test_registration_status
```

## Key Findings

1. ✅ Skeleton apps DO start and try to register
2. ✅ Skeleton apps successfully connect and send registration requests
3. ❌ Main app receives registration messages but doesn't broadcast them
4. ❌ HTTPService doesn't subscribe to registration events
5. ❌ /status endpoint shows empty registered_apps even when apps are connected

## What's Next

Implement the message bus communication between AppRegistrationService and HTTPService to fix the `/status` endpoint visibility of registered apps.
