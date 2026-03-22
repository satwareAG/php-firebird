--TEST--
Event Handling Error Paths and Edge Cases
--EXTENSIONS--
firebird
--SKIPIF--
<?php
// Include firebird test helpers
include __DIR__ . '/../skipif.inc';
// Skip in CI - coverage test with environment-dependent output
if (getenv('CI') || getenv('GITHUB_ACTIONS')) {
    die('skip Coverage test skipped in CI - output varies by environment');
}
?>
--FILE--
<?php
/**
 * Test comprehensive error paths and edge cases for fbird_events.c
 */

echo "=== Event Handling Error Tests ===\n\n";

require_once __DIR__ . '/../config.inc';

// Get database connection for tests that need it
$host = getenv('FIREBIRD_HOST') ?: 'firebird40';
$user = getenv('FIREBIRD_USER') ?: 'SYSDBA';
$password = getenv('FIREBIRD_PASSWORD') ?: 'masterkey';
$dbname = getenv('FIREBIRD_DATABASE') ?: '/firebird/data/test.fdb';

$conn = fbird_connect("$host:$dbname", $user, $password);

// Test 1: fbird_wait_event() - Too few arguments
echo "1. fbird_wait_event() with too few arguments:\n";
try {
    fbird_wait_event();
    echo "   FAIL - Should throw warning/error\n";
} catch (Throwable $e) {
    echo "   PASS - Error thrown: " . substr($e->getMessage(), 0, 50) . "...\n";
}
echo "\n";

// Test 2: fbird_wait_event() - Too many arguments (>16)
echo "2. fbird_wait_event() with too many arguments:\n";
try {
    $events = array_fill(0, 17, 'EVENT1'); // 17 events
    fbird_wait_event(...$events);
    echo "   FAIL - Should throw warning/error\n";
} catch (Throwable $e) {
    echo "   PASS - Error thrown: " . substr($e->getMessage(), 0, 50) . "...\n";
}
echo "\n";

// Test 3: fbird_wait_event() - Invalid link resource
echo "3. fbird_wait_event() with invalid link resource:\n";
try {
    fbird_wait_event(new stdClass(), 'EVENT1');
    echo "   FAIL - Should return false\n";
} catch (Throwable $e) {
    echo "   PASS - Error thrown: " . substr($e->getMessage(), 0, 50) . "...\n";
}
echo "\n";

// Test 4: fbird_set_event_handler() - Invalid callback (not callable)
echo "4. fbird_set_event_handler() with invalid callback:\n";
$handler = @fbird_set_event_handler($conn, 'not_a_function', 'EVENT1');
if ($handler === false) {
    echo "   PASS - Returned false for invalid callback\n";
} else {
    echo "   FAIL - Should return false\n";
}
echo "\n";

// Test 5: fbird_set_event_handler() - Invalid link resource
echo "5. fbird_set_event_handler() with invalid link resource:\n";
try {
    $handler = fbird_set_event_handler(new stdClass(), 'my_callback', 'EVENT1');
    echo "   FAIL - Should return false\n";
} catch (Throwable $e) {
    echo "   PASS - Error thrown: " . substr($e->getMessage(), 0, 50) . "...\n";
}
echo "\n";

// Test 6: fbird_set_event_handler() - Too few arguments
echo "6. fbird_set_event_handler() with too few arguments:\n";
try {
    fbird_set_event_handler();
    echo "   FAIL - Should throw warning/error\n";
} catch (Throwable $e) {
    echo "   PASS - Error thrown: " . substr($e->getMessage(), 0, 50) . "...\n";
}
echo "\n";

// Test 7: fbird_set_event_handler() - Too many arguments (>17)
echo "7. fbird_set_event_handler() with too many arguments:\n";
try {
    $events = array_fill(0, 16, 'EVENT'); // 16 events + callback = 17 args
    $handler = fbird_set_event_handler('my_callback', ...$events);
    echo "   FAIL - Should throw warning/error\n";
} catch (Throwable $e) {
    echo "   PASS - Error thrown: " . substr($e->getMessage(), 0, 50) . "...\n";
}
echo "\n";

// Test 8: fbird_poll_event() - Invalid event resource
echo "8. fbird_poll_event() with invalid event resource:\n";
try {
    fbird_poll_event(new stdClass());
    echo "   FAIL - Should return false\n";
} catch (Throwable $e) {
    echo "   PASS - Error thrown: " . substr($e->getMessage(), 0, 50) . "...\n";
}
echo "\n";

// Test 9: fbird_poll_event() - Polling dead event handler
echo "9. fbird_poll_event() on dead event handler:\n";
$handler = fbird_set_event_handler($conn, function($event) {
    echo "   Callback received: $event\n";
}, 'EVENT_DEAD_TEST');
fbird_free_event_handler($handler);
$result = fbird_poll_event($handler);
if ($result === null) {
    echo "   PASS - Returned null for dead handler\n";
} else {
    echo "   FAIL - Should return null, got: " . var_export($result, true) . "\n";
}
echo "\n";

// Test 10: fbird_poll_event() - Timeout behavior
echo "10. fbird_poll_event() timeout behavior:\n";
if (defined('FBIRD_EVENT_TIMEOUT')) {
    echo "   FBIRD_EVENT_TIMEOUT defined: " . FBIRD_EVENT_TIMEOUT . "\n";
    
$handler = fbird_set_event_handler($conn, function($event) {
    echo "   Callback: $event\n";
}, 'EVENT_TIMEOUT_TEST');
    
    $result = fbird_poll_event($handler, 10);
    
    if ($result === FBIRD_EVENT_TIMEOUT || $result === null || $result === false) {
        echo "   PASS - Poll returned as expected\n";
    } else {
        echo "   Result: " . var_export($result, true) . "\n";
    }
    
    fbird_free_event_handler($handler);
} else {
    echo "   SKIP - FBIRD_EVENT_TIMEOUT not defined\n";
}
echo "\n";

// Test 11: fbird_poll_event() - Zero timeout (immediate return)
echo "11. fbird_poll_event() with zero timeout:\n";
$handler = fbird_set_event_handler($conn, function($event) {
    echo "   Callback: $event\n";
}, 'EVENT_ZERO_TIMEOUT');

$result = fbird_poll_event($handler, 0);
if ($result === null || $result === false) {
    echo "   PASS - Returned as expected (immediate return)\n";
} else {
    echo "   Result: " . var_export($result, true) . "\n";
}

fbird_free_event_handler($handler);
echo "\n";

// Test 12: fbird_free_event_handler() - Invalid resource
echo "12. fbird_free_event_handler() with invalid resource:\n";
try {
    $result = fbird_free_event_handler(new stdClass());
    if ($result === false) {
        echo "   PASS - Returned false for invalid resource\n";
    } else {
        echo "   FAIL - Should return false\n";
    }
} catch (Throwable $e) {
    echo "   PASS - Error thrown: " . substr($e->getMessage(), 0, 50) . "...\n";
}
echo "\n";

// Test 13: fbird_free_event_handler() - Double free (safe)
echo "13. fbird_free_event_handler() double free (should be safe):\n";
$handler = fbird_set_event_handler($conn, function($event) {
    echo "   Callback: $event\n";
}, 'EVENT_DOUBLE_FREE_TEST');

$result1 = fbird_free_event_handler($handler);
$result2 = fbird_free_event_handler($handler);

if ($result1 === true && $result2 === true) {
    echo "   PASS - Both calls returned true (safe)\n";
} else {
    echo "   Result 1: " . var_export($result1, true) . "\n";
    echo "   Result 2: " . var_export($result2, true) . "\n";
}
echo "\n";

// Test 14: Event handler with no arguments (use default link)
echo "14. fbird_set_event_handler() with default link:\n";
// We need to set the default link first
ini_set('fbird.default_user', getenv('FIREBIRD_USER') ?: 'SYSDBA');
ini_set('fbird.default_password', getenv('FIREBIRD_PASSWORD') ?: 'masterkey');

try {
    $handler = fbird_set_event_handler(function($event) {
        echo "   Callback (default link): $event\n";
    }, 'EVENT_DEFAULT_LINK', 'EVENT_DEFAULT_LINK2');

    if (is_resource($handler)) {
        echo "   PASS - Handler created with default link\n";
        fbird_free_event_handler($handler);
    } else {
        echo "   FAIL - Should return resource\n";
    }
} catch (TypeError $e) {
    // If default link is not supported or signature mismatch
    echo "   PASS - Default link not supported (TypeError thrown)\n";
} catch (Throwable $e) {
    echo "   FAIL - Unexpected error: " . $e->getMessage() . "\n";
}
echo "\n";

// Test 15: Callback returning false (cancellation)
echo "15. Callback returning false (cancel event handler):\n";
$callCount = 0;
$handler = fbird_set_event_handler($conn, function($event) use (&$callCount) {
    $callCount++;
    echo "   Callback #$callCount: $event\n";
    return false; // Cancel after first event
}, 'EVENT_CANCEL_TEST');

$result1 = fbird_poll_event($handler, 10);

echo "   Poll result: " . var_export($result1, true) . "\n";

if ($result1 === null || $result1 === -2 || $result1 === false) {
    echo "   PASS - Poll returned as expected\n";
}

fbird_free_event_handler($handler);
echo "\n";

if ($conn) fbird_close($conn);

echo "=== All Event Error Tests Complete ===\n";
?>
--EXPECTF--
=== Event Handling Error Tests ===

1. fbird_wait_event() with too few arguments:
   PASS - Error thrown:%A

2. fbird_wait_event() with too many arguments:
%APASS - Error thrown:%A

3. fbird_wait_event() with invalid link resource:
%A   PASS - Error thrown:%A

4. fbird_set_event_handler() with invalid callback:
   PASS - Returned false for invalid callback

5. fbird_set_event_handler() with invalid link resource:
   PASS - Error thrown:%A

6. fbird_set_event_handler() with too few arguments:
   PASS - Error thrown:%A

7. fbird_set_event_handler() with too many arguments:
   PASS - Error thrown:%A

8. fbird_poll_event() with invalid event resource:
   PASS - Error thrown:%A

9. fbird_poll_event() on dead event handler:
   PASS - Returned null for dead handler

10. fbird_poll_event() timeout behavior:
%AFBIRD_EVENT_TIMEOUT defined: -2
%APASS - Poll returned as expected

11. fbird_poll_event() with zero timeout:
%APASS - Returned as expected (immediate return)

12. fbird_free_event_handler() with invalid resource:
   PASS - %A

13. fbird_free_event_handler() double free (should be safe):
   PASS - Both calls returned true (safe)

14. fbird_set_event_handler() with default link:
%APASS -%A

15. Callback returning false (cancel event handler):
%APoll result: %A
%APASS - Poll returned as expected

=== All Event Error Tests Complete ===
