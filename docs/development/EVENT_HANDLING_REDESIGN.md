# Event Handling Redesign for PHP 8.1+ Thread Safety

## Problem Analysis

### Current Architecture (Broken)

The current `fbird_set_event_handler()` implementation uses Firebird's async event notification via `isc_que_events()`:

```c
isc_que_events(status, &db, &event_id, buffer_size, 
               event_buffer, callback_fn, user_data);
```

When a database event fires, **Firebird's client library calls `callback_fn` from its own internal thread** - NOT from PHP's request thread.

### Why This Is Unsafe

1. **PHP is single-threaded by design**: The PHP runtime assumes all execution happens in a single thread within each request. Calling PHP functions from another thread violates this assumption.

2. **No TSRM context**: PHP 8.1+ uses Thread-Safe Resource Manager (TSRM) for thread-local storage, but the Firebird callback thread has no PHP context - no request globals, no memory manager, no error handlers.

3. **TSRMLS macros are now empty**: The legacy `TSRMLS_FETCH_FROM_CTX` / `TSRMLS_SET_CTX` macros that the old code tried to use are now **empty macros** in PHP 8.1+:
   ```c
   #define FBIRD_TSRMLS_FETCH_FROM_CTX(user_data)  // Does nothing!
   #define FBIRD_TSRMLS_SET_CTX(user_data)         // Does nothing!
   ```

4. **Memory corruption**: `call_user_function()` from a non-PHP thread accesses PHP's global state (symbol tables, object store, etc.) without synchronization, causing data races.

5. **Unpredictable crashes**: Sometimes works, sometimes segfaults, sometimes corrupts memory silently - classic threading bugs.

### Research Conclusions

From researching PHP extension development patterns (libevent, libuv, swoole):

> **"Never call PHP functions from external threads; always queue results and poll from the main PHP execution context."**

The safe pattern used by all well-designed async PHP extensions is:
1. External library callback stores result in a **thread-safe queue**
2. PHP code **polls the queue** from its own thread
3. PHP callbacks execute in the **correct PHP thread context**

## Solution: Polling-Based Event Model

### Design Overview

Instead of async callbacks, we implement a **polling model**:

1. `fbird_set_event_handler()` - Register interest in events (stores callback, no async notification)
2. `fbird_wait_event()` - Blocking wait for events (already works correctly)
3. **NEW** `fbird_poll_event()` - Non-blocking check for events, calls PHP callback if found

### New API

```php
// Register handler (stores callback, doesn't block)
$event = fbird_set_event_handler($conn, 'my_callback', 'EVENT1', 'EVENT2');

// Poll for events (non-blocking), returns:
// - false: error
// - null: no events pending
// - string: event name that fired (callback already called)
while ($result = fbird_poll_event($event)) {
    if ($result === null) {
        usleep(100000); // 100ms sleep, then poll again
        continue;
    }
    // Event fired, callback was already invoked
    echo "Event processed: $result\n";
}

// Clean up
fbird_free_event_handler($event);
```

### Implementation Strategy

#### Step 1: Remove Async Callback Registration

In `fbird_set_event_handler()`, don't call `isc_que_events()` with a callback. Instead:
- Store the event registration data
- Store the PHP callback reference
- Return an event resource handle

#### Step 2: Implement Polling Function

New `fbird_poll_event()` function:
1. Use `isc_wait_for_event()` with a very short timeout (non-blocking via a flag/select mechanism)
2. If event fired, call the PHP callback **from within the PHP thread**
3. Re-register for next event if callback returns true

#### Step 3: Alternative - Blocking with Timeout

Since Firebird's `isc_wait_for_event()` is inherently blocking, we can:
- Use a reasonable timeout (e.g., 100ms)
- Allow PHP to regain control periodically
- User code wraps in a loop

### Code Changes Required

#### Modified fbird_events.c

1. **Remove `_php_fbird_callback()`** - Delete the unsafe async callback function entirely

2. **Modify `fbird_set_event_handler()`**:
   - Don't call `isc_que_events()` 
   - Just store event registration and callback
   - Mark event state as REGISTERED (not ACTIVE)

3. **Add `fbird_poll_event()`**:
   ```c
   PHP_FUNCTION(fbird_poll_event) {
       // 1. Check if events are pending using isc_wait_for_event with short timeout
       // 2. If event fired, get event counts
       // 3. Call PHP callback from THIS thread (safe!)
       // 4. Return event name or null
   }
   ```

4. **Update `fbird_wait_event()`** - Already works correctly (blocking, in PHP thread)

### Benefits

1. **Thread-safe**: All PHP callbacks execute in PHP's request thread
2. **Predictable**: No race conditions or random crashes
3. **Debuggable**: Stack traces make sense, gdb works properly
4. **PHP 8.1+ compatible**: No reliance on deprecated TSRM patterns
5. **Testable**: Tests can run reliably without intermittent failures

### Migration Guide

Old code:
```php
// Register handler and forget - events call callback automatically
fbird_set_event_handler($conn, 'callback', 'EVENT1');
// ... do other work, callbacks fire in background ...
```

New code:
```php
// Register handler
$ev = fbird_set_event_handler($conn, 'callback', 'EVENT1');

// Must poll for events explicitly
while ($running) {
    $result = fbird_poll_event($ev, 100); // 100ms timeout
    if ($result === false) break; // Error
    // null = no event, string = event name (callback was called)
    
    // Do other work here
}

fbird_free_event_handler($ev);
```

### Alternative: Simplified Model

For simplest implementation, we can:
1. Keep `fbird_wait_event()` as-is (blocking sync, works fine)
2. **Deprecate** `fbird_set_event_handler()` for async usage
3. Document that async event patterns should use `fbird_wait_event()` in a loop

This is actually what most database event systems do - the application maintains its own event loop.

## Implementation Plan

### Phase 1: Simple Fix (PR-ready)

1. Update `fbird_set_event_handler()` to NOT use async callbacks
2. Add `fbird_poll_event()` for non-blocking event checking
3. Remove the unsafe `_php_fbird_callback()` function
4. Update tests to use polling pattern

### Phase 2: Documentation

1. Update README with new event handling patterns
2. Add migration guide for users upgrading from legacy async
3. Document thread-safety guarantees

### Phase 3: Test Fixes

1. Remove skip conditions from tests/008.phpt
2. Remove skip conditions from tests/bug45575.phpt
3. Add comprehensive event handling tests

## Technical Details

### Firebird Event API Reference

```c
// Register for async event notification (with callback - UNSAFE for PHP)
ISC_STATUS isc_que_events(
    ISC_STATUS *status_vector,
    isc_db_handle *db_handle,
    ISC_LONG *event_id,
    ISC_USHORT length,
    const ISC_UCHAR *event_buffer,
    ISC_EVENT_CALLBACK callback,  // Called from Firebird's thread!
    void *callback_arg
);

// Wait synchronously for event (blocks until event fires - SAFE)
ISC_STATUS isc_wait_for_event(
    ISC_STATUS *status_vector,
    isc_db_handle *db_handle,
    ISC_USHORT length,
    const ISC_UCHAR *event_buffer,
    ISC_UCHAR *result_buffer
);

// Cancel pending async event registration
ISC_STATUS isc_cancel_events(
    ISC_STATUS *status_vector,
    isc_db_handle *db_handle,
    ISC_LONG *event_id
);

// Get event occurrence counts
void isc_event_counts(
    ISC_ULONG *result_vector,
    ISC_USHORT buffer_length,
    const ISC_UCHAR *event_buffer,
    const ISC_UCHAR *result_buffer
);
```

### Key Insight

`isc_wait_for_event()` is the **synchronous** alternative to `isc_que_events()`. It blocks the calling thread until an event fires.

For PHP, synchronous blocking in a polling loop is **safer** than async callbacks from foreign threads.
