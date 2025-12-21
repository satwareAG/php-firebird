--TEST--
fbird_get_limbo_transactions() / fbird_reconnect_transaction(): Limbo transaction recovery
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

echo "=== Test 1: Get limbo transactions on clean database ===\n";
$x = fbird_connect($test_base);

$limbo_ids = fbird_get_limbo_transactions($x);
var_dump($limbo_ids);

if ($limbo_ids === false) {
    echo "Error: " . fbird_errmsg() . "\n";
} elseif (is_array($limbo_ids) && count($limbo_ids) === 0) {
    echo "No limbo transactions found (expected)\n";
}

echo "\n=== Test 2: Get limbo transactions with max_count parameter ===\n";
$limbo_ids = fbird_get_limbo_transactions($x, 50);
var_dump($limbo_ids);

if (is_array($limbo_ids)) {
    echo "Count: " . count($limbo_ids) . "\n";
}

echo "\n=== Test 3: Invalid max_count values ===\n";
$result = @fbird_get_limbo_transactions($x, 0);
var_dump($result);

$result = @fbird_get_limbo_transactions($x, 10001);
var_dump($result);

echo "\n=== Test 4: Reconnect to non-existent transaction (should fail) ===\n";
$reconnected = @fbird_reconnect_transaction($x, 999999);
var_dump($reconnected);

if ($reconnected === false) {
    echo "Failed as expected\n";
}

fbird_close($x);

echo "\n=== Test 5: Error handling - invalid parameters ===\n";
$result = @fbird_get_limbo_transactions(null);
var_dump($result);

echo "\nDone\n";

?>
--EXPECTF--
=== Test 1: Get limbo transactions on clean database ===
array(0) {
}
No limbo transactions found (expected)

=== Test 2: Get limbo transactions with max_count parameter ===
array(0) {
}
Count: 0

=== Test 3: Invalid max_count values ===

Warning: fbird_get_limbo_transactions(): max_count must be between 1 and 10000 in %s on line %d
bool(false)

Warning: fbird_get_limbo_transactions(): max_count must be between 1 and 10000 in %s on line %d
bool(false)

=== Test 4: Reconnect to non-existent transaction (should fail) ===
bool(false)
Failed as expected

=== Test 5: Error handling - invalid parameters ===

Warning: fbird_get_limbo_transactions() expects at most 2 arguments, 1 given in %s on line %d
NULL

Done
