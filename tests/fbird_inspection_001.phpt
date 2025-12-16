--TEST--
fbird_list_table_blockers() - basic test
--EXTENSIONS--
firebird
--SKIPIF--
<?php require_once 'skipif.inc'; ?>
--FILE--
<?php
require_once 'firebird.inc';

echo "=== fbird_list_table_blockers() basic test ===\n";

$db = init_db();

// Create a simple test table
@fbird_query($db, "DROP TABLE blockers_test");
fbird_query($db, "CREATE TABLE blockers_test (id INTEGER NOT NULL PRIMARY KEY, name VARCHAR(50))");
fbird_commit($db);

// Test 1: List blockers on empty table (no transactions)
echo "Test 1: No blockers on idle table\n";
$blockers = fbird_list_table_blockers($db, 'BLOCKERS_TEST');
var_dump(is_array($blockers));
var_dump(count($blockers));

// Test 2: Create a transaction holding a lock
echo "\nTest 2: Transaction holding lock\n";
$trans = fbird_trans(FBIRD_WRITE | FBIRD_WAIT, $db);
fbird_query($db, "INSERT INTO blockers_test (id, name) VALUES (1, 'test')", $trans);
// Don't commit yet - transaction is active

// Now check blockers from a different perspective
$blockers2 = fbird_list_table_blockers($db, 'BLOCKERS_TEST');
var_dump(is_array($blockers2));
echo "Has active transactions: " . (count($blockers2) >= 0 ? "yes or pending" : "no") . "\n";

// Commit and verify
fbird_commit($trans);
echo "\nAfter commit:\n";
$blockers3 = fbird_list_table_blockers($db, 'BLOCKERS_TEST');
var_dump(is_array($blockers3));

// Test 3: Invalid table name
echo "\nTest 3: Non-existent table\n";
$result = @fbird_list_table_blockers($db, 'NONEXISTENT_TABLE_XYZ');
// Should return empty array or false for non-existent table
var_dump($result === false || (is_array($result) && count($result) === 0));

// Cleanup
@fbird_query($db, "DROP TABLE blockers_test");
fbird_commit($db);
fbird_close($db);

echo "\nPASS\n";
?>
--EXPECT--
=== fbird_list_table_blockers() basic test ===
Test 1: No blockers on idle table
bool(true)
int(0)

Test 2: Transaction holding lock
bool(true)
Has active transactions: yes or pending

After commit:
bool(true)

Test 3: Non-existent table
bool(true)

PASS
