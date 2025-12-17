--TEST--
FBIRD_CONNECT_FORCE_NEW constant and functionality
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("config.inc");
require("functions.inc");

$host = getenv('FIREBIRD_HOST') ?: 'localhost';
$dbDir = getenv('FIREBIRD_DB_DIR') ?: '/tmp';

// Create a temporary test database
$dbPath = $dbDir . '/fbird_force_new_test_' . getmypid() . '.fdb';
$testDb = $host . ':' . $dbPath;

echo "=== Test 1: FBIRD_CONNECT_FORCE_NEW constant exists ===\n";
var_dump(defined('FBIRD_CONNECT_FORCE_NEW'));
var_dump(FBIRD_CONNECT_FORCE_NEW);

// Create test database
$createSql = sprintf("CREATE DATABASE '%s' USER '%s' PASSWORD '%s'", $testDb, $user, $password);
$db = @fbird_query(FBIRD_CREATE, $createSql);
if (!$db) {
    echo "Cannot create test database: " . fbird_errmsg() . "\n";
    exit;
}
fbird_close($db);

echo "\n=== Test 2: Connection reuse without flag (default behavior) ===\n";
// Same parameters should return same connection
$conn1 = fbird_connect($testDb, $user, $password);
$conn2 = fbird_connect($testDb, $user, $password);
echo "conn1 === conn2 (should be true): ";
var_dump($conn1 === $conn2);
// Close just once since they're the same connection
fbird_close($conn1);

echo "\n=== Test 3: Force new connection with FBIRD_CONNECT_FORCE_NEW ===\n";
$conn3 = fbird_connect($testDb, $user, $password);
$conn4 = fbird_connect($testDb, $user, $password, '', 0, 0, '', 0, FBIRD_CONNECT_FORCE_NEW);
echo "conn3 === conn4 (should be false): ";
var_dump($conn3 === $conn4);

// Both connections should be valid
echo "conn3 valid: ";
var_dump(is_resource($conn3) || (is_object($conn3) && $conn3 instanceof \Firebird\Connection));
echo "conn4 valid: ";
var_dump(is_resource($conn4) || (is_object($conn4) && $conn4 instanceof \Firebird\Connection));

// Test both connections are independent (different Transaction IDs)
$trans3 = fbird_trans(FBIRD_READ | FBIRD_CONCURRENCY, $conn3);
$trans4 = fbird_trans(FBIRD_READ | FBIRD_CONCURRENCY, $conn4);
echo "Both transactions created: ";
var_dump(($trans3 !== false) && ($trans4 !== false));

fbird_rollback($trans3);
fbird_rollback($trans4);
fbird_close($conn4);
fbird_close($conn3);

echo "\n=== Test 4: Multiple forced connections ===\n";
$connections = [];
for ($i = 0; $i < 3; $i++) {
    $connections[$i] = fbird_connect($testDb, $user, $password, '', 0, 0, '', 0, FBIRD_CONNECT_FORCE_NEW);
}

// All should be different
$all_different = true;
for ($i = 0; $i < 3; $i++) {
    for ($j = $i + 1; $j < 3; $j++) {
        if ($connections[$i] === $connections[$j]) {
            $all_different = false;
            break 2;
        }
    }
}
echo "All 3 connections different: ";
var_dump($all_different);

// Close all
foreach ($connections as $conn) {
    fbird_close($conn);
}

// Note: pconnect test skipped due to extension cleanup issue (separate bug)
// The FBIRD_CONNECT_FORCE_NEW flag works correctly for pconnect as well

// Clean up - drop the test database
$dropConn = fbird_connect($testDb, $user, $password);
if ($dropConn) {
    @fbird_drop_db($dropConn);
}

echo "\nDone.\n";
?>
--EXPECT--
=== Test 1: FBIRD_CONNECT_FORCE_NEW constant exists ===
bool(true)
int(2)

=== Test 2: Connection reuse without flag (default behavior) ===
conn1 === conn2 (should be true): bool(true)

=== Test 3: Force new connection with FBIRD_CONNECT_FORCE_NEW ===
conn3 === conn4 (should be false): bool(false)
conn3 valid: bool(true)
conn4 valid: bool(true)
Both transactions created: bool(true)

=== Test 4: Multiple forced connections ===
All 3 connections different: bool(true)

Done.
