--TEST--
fbird_gen_id() - basic generator operations
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

echo "=== fbird_gen_id() basic test ===\n";

$db = fbird_connect($test_base);
if (!$db) {
    die("Could not connect to test database");
}

// Create a generator
@fbird_query($db, "DROP GENERATOR test_gen_id");
fbird_query($db, "CREATE GENERATOR test_gen_id");
fbird_commit($db);

// Test 1: Get next value with default increment (1)
echo "Test 1: Default increment\n";
$val1 = fbird_gen_id("test_gen_id", 1, $db);
var_dump(is_int($val1) || is_float($val1));
echo "Value: $val1\n";

// Test 2: Get next value with custom increment
echo "\nTest 2: Custom increment (5)\n";
$val2 = fbird_gen_id("test_gen_id", 5, $db);
var_dump($val2 === $val1 + 5);
echo "Value: $val2 (expected: " . ($val1 + 5) . ")\n";

// Test 3: Increment by 0 (peek without advancing)
echo "\nTest 3: Increment by 0 (peek)\n";
$val3 = fbird_gen_id("test_gen_id", 0, $db);
var_dump($val3 === $val2);
echo "Value stays at: $val3\n";

// Test 4: Negative increment (decrement)
echo "\nTest 4: Negative increment (-1)\n";
$val4 = fbird_gen_id("test_gen_id", -1, $db);
var_dump($val4 === $val3 - 1);
echo "Value: $val4\n";

// Test 5: Large increment
echo "\nTest 5: Large increment (100)\n";
$val5 = fbird_gen_id("test_gen_id", 100, $db);
var_dump($val5 === $val4 + 100);
echo "Value: $val5\n";

// Test 6: Using default link (no $db parameter)
echo "\nTest 6: Default link\n";
$val6 = fbird_gen_id("test_gen_id", 1);
var_dump(is_int($val6) || is_float($val6));
var_dump($val6 === $val5 + 1);

// Cleanup
@fbird_query($db, "DROP GENERATOR test_gen_id");
fbird_commit($db);
fbird_close($db);

echo "\nPASS\n";
?>
--EXPECT--
=== fbird_gen_id() basic test ===
Test 1: Default increment
bool(true)
Value: 1

Test 2: Custom increment (5)
bool(true)
Value: 6 (expected: 6)

Test 3: Increment by 0 (peek)
bool(true)
Value stays at: 6

Test 4: Negative increment (-1)
bool(true)
Value: 5

Test 5: Large increment (100)
bool(true)
Value: 105

Test 6: Default link
bool(true)
bool(true)

PASS

--CLEAN--
<?php
require_once 'firebird.inc';
$db = @fbird_connect($test_base, $user, $password);
if ($db) {
    @fbird_query($db, "DROP GENERATOR test_gen_id");
    @fbird_close($db);
}
?>
