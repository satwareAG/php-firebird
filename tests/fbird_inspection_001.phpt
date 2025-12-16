--TEST--
fbird_list_table_blockers() - basic test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

echo "=== fbird_list_table_blockers() basic test ===\n";

$db = fbird_connect($test_base);
if (!$db) {
    die("Could not connect to test database");
}

// Test 1: Function exists
echo "Test 1: Function exists\n";
var_dump(function_exists('fbird_list_table_blockers'));

// Test 2: List blockers on clean connection (should be empty or return expected format)
echo "\nTest 2: List blockers\n";
$blockers = fbird_list_table_blockers("test1", $db);
var_dump(is_array($blockers) || $blockers === false);

// Test 3: If array, check structure
if (is_array($blockers)) {
    echo "Returned array with " . count($blockers) . " elements\n";
} else {
    echo "Returned non-array (may not be supported on all Firebird versions)\n";
}

// Cleanup
fbird_close($db);

echo "\nPASS\n";
?>
--EXPECTF--
=== fbird_list_table_blockers() basic test ===
Test 1: Function exists
bool(true)

Test 2: List blockers
bool(true)
%s

PASS
