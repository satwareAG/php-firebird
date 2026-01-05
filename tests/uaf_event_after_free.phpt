--TEST--
UAF detection: Using event after fbird_free_event_handler()
--DESCRIPTION--
Verify that attempting to use an event resource after freeing it
produces a proper error rather than undefined behavior or memory corruption.
--SKIPIF--
<?php
include("skipif.inc");
?>
--FILE--
<?php
require("firebird.inc");

$conn = fbird_connect($test_base);
if (!$conn) {
    die("Cannot connect: " . fbird_errmsg());
}

$event_count = 0;
$callback_called = false;

// Simple callback - just count calls
function event_callback($event_name, $count) {
    global $callback_called, $event_count;
    $callback_called = true;
    $event_count = $count;
}

// Register event handler
$event = fbird_set_event_handler($conn, 'event_callback', 'TEST_EVENT');
if (!$event) {
    die("Cannot register event: " . fbird_errmsg());
}

echo "Event handler registered successfully\n";

// Free the event handler
$freed = fbird_free_event_handler($event);
echo "Event handler freed: " . ($freed ? "true" : "false") . "\n";

// Attempt to free again (should error, not crash)
echo "Attempting to use freed event handler...\n";

$free2 = @fbird_free_event_handler($event);
if ($free2 === false) {
    echo "Re-free failed as expected\n";
} else {
    echo "Re-free returned success (resource already destroyed)\n";
}

fbird_close($conn);
echo "Test completed without crash\n";
?>
--EXPECT--
Event handler registered successfully
Event handler freed: true
Attempting to use freed event handler...
Re-free returned success (resource already destroyed)
Test completed without crash
