--TEST--
Bug #45575 (Error handling for invalid callback in fbird_set_event_handler)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
/**
 * Test that fbird_set_event_handler() properly validates the callback argument.
 *
 * The callback must be a valid callable function. Invalid values should
 * produce appropriate warning messages and return false.
 */

require("firebird.inc");

$db = fbird_connect($test_base, $user, $password);
if (!$db) die("Connection failed");

function valid_callback($event) { return true; }

echo "Test 1: null callback\n";
$result = @fbird_set_event_handler($db, null, 'TEST1');
var_dump($result);

echo "\nTest 2: integer callback\n";
$result = @fbird_set_event_handler($db, 1, 'TEST1');
var_dump($result);

echo "\nTest 3: non-existent function callback\n";
$result = @fbird_set_event_handler($db, 'nonexistent_function', 'TEST1');
var_dump($result);

echo "\nTest 4: valid callback (should succeed)\n";
$result = fbird_set_event_handler($db, 'valid_callback', 'TEST1');
if (is_resource($result) || $result instanceof \Firebird\Event) {
    echo "OK: Event handler registered\n";
    fbird_free_event_handler($result);
} else {
    echo "FAIL: Expected resource, got " . gettype($result) . "\n";
}

echo "\nTest 5: closure callback (should succeed)\n";
$result = fbird_set_event_handler($db, function($event) { return true; }, 'TEST1');
if (is_resource($result) || $result instanceof \Firebird\Event) {
    echo "OK: Event handler registered with closure\n";
    fbird_free_event_handler($result);
} else {
    echo "FAIL: Expected resource, got " . gettype($result) . "\n";
}

fbird_close($db);
echo "\nDone\n";
?>
--EXPECTF--
Test 1: null callback
bool(false)

Test 2: integer callback
bool(false)

Test 3: non-existent function callback
bool(false)

Test 4: valid callback (should succeed)
OK: Event handler registered

Test 5: closure callback (should succeed)
OK: Event handler registered with closure

Done
