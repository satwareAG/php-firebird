# RFC: Event Handling Timeout and Non-Blocking Options

**Status:** PHASE 2 COMPLETE  
**Author:** Michael Wegener  
**Date:** December 2025  
**Related:** EVENT_HANDLING_REDESIGN.md

## Implementation Status

### Phase 1: C Extension Enhancement (December 11, 2025)

**Status: PARTIALLY IMPLEMENTED** - API complete but timeout mechanism non-functional

1. **API Enhancement:**
   - `fbird_poll_event()` now accepts optional `int $timeout_ms` parameter
   - `FBIRD_EVENT_TIMEOUT` constant defined (value: -2)
   - `IBASE_EVENT_TIMEOUT` alias for compatibility
   - Arginfo updated with proper type hints and default value

2. **Timeout Infrastructure (Unix):**
   - Signal handler for `SIGALRM` installed
   - `alarm()` setup/teardown around `isc_wait_for_event()`
   - Previous alarm state preserved and restored

### Phase 2: PHP Wrapper Classes (December 11, 2025)

**Status: IMPLEMENTED** - Full PHP wrapper class library

1. **Interface and Factory:**
   - `EventPollerInterface` - Standard interface for all strategies
   - `EventPoller` - Factory with auto-detection and strategy selection

2. **Strategy Implementations:**
   - `ProcessEventPoller` - Most reliable, uses process isolation via `proc_open()`
   - `PcntlEventPoller` - Signal-based (SIGALRM), may have same limitations as C
   - `FiberEventPoller` - Async integration with AMPHP (requires external package)

3. **Files Added:**
   - `src/Firebird/EventPollerInterface.php`
   - `src/Firebird/EventPoller.php`
   - `src/Firebird/ProcessEventPoller.php`
   - `src/Firebird/PcntlEventPoller.php`
   - `src/Firebird/FiberEventPoller.php`
   - `tests/event_poller_wrapper.phpt`

4. **Usage Example:**
   ```php
   use Firebird\EventPoller;
   
   $conn = fbird_connect($database, $user, $password);
   $event = fbird_set_event_handler($conn, $callback, 'MY_EVENT');
   
   // Auto-detect best strategy
   $poller = EventPoller::create($event);
   
   // For process-based timeout (most reliable):
   $poller = EventPoller::create($event, 'process');
   $poller->setConnectionDetails($database, $user, $password, ['MY_EVENT'], $callback);
   
   // Poll with 5 second timeout
   $result = $poller->poll(5000);
   if ($result === FBIRD_EVENT_TIMEOUT) {
       echo "Timeout reached\n";
   }
   
   $poller->free();
   ```

### Critical Limitation Discovered

**The Firebird client library's `isc_wait_for_event()` is NOT interruptible by signals on Linux.**

Testing revealed that `SIGALRM` does not interrupt the blocking `isc_wait_for_event()` call.
The Firebird client library likely uses internal mechanisms that either:
- Restart interrupted system calls automatically
- Use signal-resistant blocking primitives
- Handle `EINTR` internally and retry

This means the timeout parameter is currently **non-functional** for actual timeout interruption,
though the API is fully in place for future alternative implementations.

### Future Implementation Options

To make timeouts functional, one of these approaches would be needed:

1. **Socket access from fbclient:** Get the underlying socket FD and use `select()`/`poll()`
   before calling `isc_wait_for_event()` (requires fbclient internals or async API)

2. **isc_que_events() approach:** Use the async event API with custom polling loop
   (more complex but doesn't block)

3. **Process isolation:** Fork a child process for blocking wait, parent monitors with timeout
   (high overhead but reliable)

4. **Thread-based:** Use pthread with timeout join
   (complex, thread-safety concerns)

## Summary

This RFC proposes multiple approaches for adding timeout support to `fbird_poll_event()`, enabling non-blocking event handling patterns. The goal is to provide flexible options that work across different PHP environments.

## Current Limitation

```php
// Current: blocks forever until event fires
$result = fbird_poll_event($ev);  // ⚠️ No timeout!
```

## Proposed API

```php
// Enhanced: with optional timeout (milliseconds)
$result = fbird_poll_event($ev, 5000);  // 5 second timeout

// Return values:
// - string: Event name that fired (callback executed)
// - null: Handler cancelled by callback
// - false: Error occurred
// - FBIRD_EVENT_TIMEOUT: Timeout reached (new constant)
```

---

## Implementation Options

### Option 1: Socket Select/Poll with Timeout (RECOMMENDED)

**Approach:** Use `select()` or `poll()` system calls with timeout on Firebird's underlying connection socket.

**Implementation in C:**
```c
PHP_FUNCTION(fbird_poll_event)
{
    zval *event_arg;
    zend_long timeout_ms = -1;  // Default: block forever
    fbird_event *event;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "r|l", &event_arg, &timeout_ms) == FAILURE) {
        RETURN_FALSE;
    }
    
    event = (fbird_event *)zend_fetch_resource_ex(event_arg, "Firebird event", le_event);
    if (!event || event->state == DEAD) {
        RETURN_FALSE;
    }
    
    if (timeout_ms >= 0) {
        // Use select with timeout
        fd_set readfds;
        struct timeval tv;
        int fd = get_fb_connection_socket(event->link);  // Need to expose this
        
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        
        int ret = select(fd + 1, &readfds, NULL, NULL, &tv);
        if (ret == 0) {
            RETURN_LONG(FBIRD_EVENT_TIMEOUT);  // Timeout
        } else if (ret < 0) {
            _php_fbird_module_error("Select error during event poll");
            RETURN_FALSE;
        }
        // Fall through to isc_wait_for_event - socket is ready
    }
    
    // Existing blocking call
    if (isc_wait_for_event(IB_STATUS, &event->link->handle.db, 
            event->buffer_size, event->event_buffer, event->result_buffer)) {
        _php_fbird_error();
        RETURN_FALSE;
    }
    
    // ... rest of current implementation
}
```

**Pros:**
- ✅ Cross-platform (Unix/Linux/Windows)
- ✅ No external dependencies
- ✅ Precise timeout control
- ✅ Efficient (no busy-waiting)
- ✅ Works in all PHP environments

**Cons:**
- ⚠️ Requires access to Firebird's connection socket FD
- ⚠️ Implementation complexity in C

**PHP Usage:**
```php
while (true) {
    $result = fbird_poll_event($ev, 1000);  // 1 second timeout
    
    if ($result === FBIRD_EVENT_TIMEOUT) {
        // Do other work during idle time
        process_background_tasks();
        continue;
    }
    
    if ($result === false) {
        break;  // Error
    }
    
    echo "Event: $result\n";
}
```

---

### Option 2: PCNTL Signal-Based Timeout

**Approach:** Use `pcntl_alarm()` to interrupt the blocking call.

**PHP Wrapper:**
```php
<?php
/**
 * Event polling with PCNTL-based timeout.
 * Requires pcntl extension.
 */
class FirebirdEventPoller
{
    private $event;
    private $timedOut = false;
    
    public function __construct($event)
    {
        if (!extension_loaded('pcntl')) {
            throw new RuntimeException('PCNTL extension required for timeout support');
        }
        $this->event = $event;
    }
    
    public function poll(int $timeoutSeconds): mixed
    {
        $this->timedOut = false;
        
        // Save old handler
        $oldHandler = pcntl_signal(SIGALRM, function() {
            $this->timedOut = true;
        });
        
        declare(ticks=1);  // Enable signal delivery
        
        try {
            pcntl_alarm($timeoutSeconds);
            $result = fbird_poll_event($this->event);  // Blocking call
            pcntl_alarm(0);  // Cancel alarm
            
            if ($this->timedOut) {
                return FBIRD_EVENT_TIMEOUT;
            }
            
            return $result;
        } finally {
            // Restore old handler
            pcntl_signal(SIGALRM, $oldHandler ?: SIG_DFL);
        }
    }
}
```

**Pros:**
- ✅ Simple implementation
- ✅ Works with current C code (no changes needed)
- ✅ Clear timeout semantics

**Cons:**
- ❌ Requires pcntl extension (not available everywhere)
- ❌ Minimum 1 second granularity
- ❌ Not thread-safe
- ❌ May conflict with other signal handlers
- ❌ Windows not supported

---

### Option 3: PHP Fibers (PHP 8.1+)

**Approach:** Wrap blocking call in a Fiber with timer-based cancellation via event loop.

**PHP Wrapper with AMPHP:**
```php
<?php
use Amp\Cancellation;
use Amp\TimeoutCancellation;
use Revolt\EventLoop;

/**
 * Async event polling using PHP Fibers.
 * Requires amphp/amp ^3.0.
 */
class AsyncFirebirdEventPoller
{
    private $event;
    
    public function __construct($event)
    {
        $this->event = $event;
    }
    
    public function pollAsync(float $timeoutSeconds): mixed
    {
        $cancellation = new TimeoutCancellation($timeoutSeconds);
        $suspension = EventLoop::getSuspension();
        
        // Start blocking call in background via thread pool
        // (requires parallel extension or process fork)
        $callback = function() use ($suspension) {
            try {
                $result = fbird_poll_event($this->event);
                $suspension->resume($result);
            } catch (Throwable $e) {
                $suspension->throw($e);
            }
        };
        
        // Schedule timeout
        $timerId = EventLoop::delay($timeoutSeconds, function() use ($suspension) {
            $suspension->resume(FBIRD_EVENT_TIMEOUT);
        });
        
        try {
            $result = $suspension->suspend();
            EventLoop::cancel($timerId);
            return $result;
        } catch (CancelledException $e) {
            return FBIRD_EVENT_TIMEOUT;
        }
    }
}
```

**Pros:**
- ✅ Modern async/await style
- ✅ Works with AMPHP/ReactPHP ecosystem
- ✅ Efficient concurrency
- ✅ True non-blocking main loop

**Cons:**
- ⚠️ Requires PHP 8.1+
- ⚠️ Requires external library (amphp/amp)
- ⚠️ Complex integration with blocking C code
- ⚠️ May need parallel extension for true async

---

### Option 4: Process-Based Isolation

**Approach:** Fork child process to handle blocking event wait.

**PHP Implementation:**
```php
<?php
/**
 * Event polling using process isolation.
 * Works without pcntl if proc_open available.
 */
class ProcessIsolatedEventPoller
{
    private $connection;
    private $events;
    
    public function poll(int $timeoutMs): array
    {
        // Create helper script that does the blocking wait
        $script = $this->createPollerScript();
        
        $descriptors = [
            0 => ['pipe', 'r'],
            1 => ['pipe', 'w'],
            2 => ['pipe', 'w']
        ];
        
        $process = proc_open(PHP_BINARY . ' ' . $script, $descriptors, $pipes);
        
        // Set timeout on stdout pipe
        stream_set_timeout($pipes[1], 0, $timeoutMs * 1000);
        
        $output = fgets($pipes[1]);
        $info = stream_get_meta_data($pipes[1]);
        
        proc_terminate($process);
        proc_close($process);
        unlink($script);
        
        if ($info['timed_out']) {
            return ['timeout' => true];
        }
        
        return json_decode($output, true);
    }
    
    private function createPollerScript(): string
    {
        $script = tempnam(sys_get_temp_dir(), 'fb_poll_') . '.php';
        file_put_contents($script, '<?php
            $conn = fbird_connect($_SERVER["FB_DB"], $_SERVER["FB_USER"], $_SERVER["FB_PASS"]);
            $ev = fbird_set_event_handler($conn, function($name) {
                echo json_encode(["event" => $name]);
                return false;
            }, ...explode(",", $_SERVER["FB_EVENTS"]));
            fbird_poll_event($ev);
        ?>');
        return $script;
    }
}
```

**Pros:**
- ✅ Works everywhere (proc_open is widely available)
- ✅ Complete isolation from main process
- ✅ No extension dependencies

**Cons:**
- ❌ High overhead (process spawn per poll)
- ❌ Complex state management
- ❌ Requires careful cleanup

---

## Comparison Matrix

| Feature | Select/Poll | PCNTL | Fibers | Process |
|---------|-------------|-------|--------|---------|
| **Implementation** | C extension | PHP wrapper | PHP wrapper | PHP wrapper |
| **Min Timeout** | 1ms | 1s | 1ms | 10ms+ |
| **Platform** | All | Unix | All | All |
| **PHP Version** | Any | Any | 8.1+ | Any |
| **Dependencies** | None | pcntl | amphp | None |
| **Thread-safe** | Yes | No | Yes | Yes |
| **Overhead** | Low | Low | Medium | High |
| **Complexity** | High (C) | Low | Medium | Medium |

---

## Recommended Implementation Plan

### Phase 1: C Extension Enhancement (Primary)

Add native timeout support to `fbird_poll_event()` using select/poll:

```c
// New function signature
PHP_FUNCTION(fbird_poll_event)  // fbird_poll_event($event, $timeout_ms = -1)
```

**Deliverables:**
1. Add `timeout_ms` parameter to `fbird_poll_event()`
2. Implement select-based timeout in C
3. Add `FBIRD_EVENT_TIMEOUT` constant
4. Update tests to cover timeout scenarios

### Phase 2: PHP Wrapper Classes (Optional)

Provide PHP wrapper classes for environments without C changes:

```php
// src/FirebirdEventPoller.php
namespace Firebird;

class EventPoller
{
    public static function create($event, string $strategy = 'auto'): EventPollerInterface
    {
        return match($strategy) {
            'pcntl' => new PcntlEventPoller($event),
            'fiber' => new FiberEventPoller($event),
            'process' => new ProcessEventPoller($event),
            'auto' => self::detectBestStrategy($event),
        };
    }
}
```

### Phase 3: Documentation

1. Update README with timeout examples
2. Add migration guide for async patterns
3. Document each strategy's trade-offs

---

## Questions for Discussion

1. **Default timeout:** Should we have a default timeout (e.g., 30 seconds) or block forever by default?

2. **Timeout constant format:** Return special constant `FBIRD_EVENT_TIMEOUT` or throw exception?

3. **PHP wrapper distribution:** Include PHP wrapper classes in extension or separate package?

4. **Backward compatibility:** Should the C implementation require explicit timeout for new behavior?

---

## References

- [PHP socket_set_nonblock](https://www.php.net/manual/en/function.socket-set-nonblock.php)
- [AMPHP Non-blocking I/O](https://amphp.org/)
- [ReactPHP Event Loop](https://reactphp.org/)
- [select(2) man page](https://man7.org/linux/man-pages/man2/select.2.html)
- [Firebird Event API](https://firebirdsql.org/file/documentation/html/en/firebirddocs/fbdevgd/firebird-database-developer-guide.html)
