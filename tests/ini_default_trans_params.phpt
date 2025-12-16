--TEST--
fbird.default_trans_params INI setting
--EXTENSIONS--
firebird
--SKIPIF--
<?php require_once 'skipif.inc'; ?>
--FILE--
<?php
require_once 'firebird.inc';

echo "=== fbird.default_trans_params INI test ===\n";

// Test 1: Check default value
echo "Test 1: Default value\n";
$default = ini_get('fbird.default_trans_params');
var_dump($default);

// Test 2: Set to FBIRD_READ + FBIRD_COMMITTED
echo "\nTest 2: Modify to READ + COMMITTED\n";
$read_committed = FBIRD_READ | FBIRD_COMMITTED;
ini_set('fbird.default_trans_params', (string)$read_committed);
var_dump(ini_get('fbird.default_trans_params'));

// Test 3: Transaction should inherit default params
echo "\nTest 3: Transaction with default params\n";
$db = init_db();

// Create test table
@fbird_query($db, "DROP TABLE trans_params_test");
fbird_query($db, "CREATE TABLE trans_params_test (id INTEGER NOT NULL PRIMARY KEY, val INTEGER)");
fbird_commit($db);

// Start transaction (should use default params)
$trans = fbird_trans($db);
var_dump($trans !== false);

// Insert data
$result = fbird_query($db, "INSERT INTO trans_params_test (id, val) VALUES (1, 100)", $trans);
var_dump($result !== false);
fbird_commit($trans);

// Verify
$result = fbird_query($db, "SELECT val FROM trans_params_test WHERE id = 1");
$row = fbird_fetch_row($result);
var_dump($row[0] == 100);
fbird_free_result($result);

// Cleanup
@fbird_query($db, "DROP TABLE trans_params_test");
fbird_commit($db);
fbird_close($db);

// Test 4: Restore default
echo "\nTest 4: Restore default\n";
ini_set('fbird.default_trans_params', '0');
var_dump(ini_get('fbird.default_trans_params'));

echo "\nPASS\n";
?>
--EXPECT--
=== fbird.default_trans_params INI test ===
Test 1: Default value
string(1) "0"

Test 2: Modify to READ + COMMITTED
string(2) "20"

Test 3: Transaction with default params
bool(true)
bool(true)
bool(true)

Test 4: Restore default
string(1) "0"

PASS
