--TEST--
fbird_pconnect() - persistent connection test
--EXTENSIONS--
firebird
--SKIPIF--
<?php require_once 'skipif.inc'; ?>
--FILE--
<?php
require_once 'firebird.inc';

echo "=== fbird_pconnect() test ===\n";

// Test 1: Check INI defaults
echo "Test 1: INI defaults\n";
var_dump(ini_get('fbird.allow_persistent'));
var_dump(ini_get('fbird.max_persistent'));
var_dump(ini_get('fbird.max_links'));

// Test 2: Basic persistent connection
echo "\nTest 2: Basic persistent connection\n";
$db1 = fbird_pconnect(TEST_DB_PATH, TEST_USER, TEST_PASS);
var_dump(is_resource($db1) || $db1 !== false);

// Test 3: Verify connection works
echo "\nTest 3: Query on persistent connection\n";
$result = fbird_query($db1, "SELECT 1 AS test_val FROM RDB\$DATABASE");
$row = fbird_fetch_assoc($result);
var_dump($row['TEST_VAL'] == 1);
fbird_free_result($result);

// Test 4: Second persistent connection to same database
echo "\nTest 4: Second persistent connection (should reuse)\n";
$db2 = fbird_pconnect(TEST_DB_PATH, TEST_USER, TEST_PASS);
var_dump(is_resource($db2) || $db2 !== false);

// Test 5: Both connections work
echo "\nTest 5: Both connections work\n";
$result1 = fbird_query($db1, "SELECT 'db1' AS source FROM RDB\$DATABASE");
$result2 = fbird_query($db2, "SELECT 'db2' AS source FROM RDB\$DATABASE");
$row1 = fbird_fetch_assoc($result1);
$row2 = fbird_fetch_assoc($result2);
var_dump($row1['SOURCE'] === 'db1');
var_dump($row2['SOURCE'] === 'db2');
fbird_free_result($result1);
fbird_free_result($result2);

// Test 6: Close connections
echo "\nTest 6: Close persistent connections\n";
$close1 = fbird_close($db1);
$close2 = fbird_close($db2);
var_dump($close1);
var_dump($close2);

// Test 7: Persistent connection with charset
echo "\nTest 7: Persistent connection with charset\n";
$db3 = fbird_pconnect(TEST_DB_PATH, TEST_USER, TEST_PASS, 'UTF8');
var_dump(is_resource($db3) || $db3 !== false);
fbird_close($db3);

echo "\nPASS\n";
?>
--EXPECT--
=== fbird_pconnect() test ===
Test 1: INI defaults
string(1) "1"
string(2) "-1"
string(2) "-1"

Test 2: Basic persistent connection
bool(true)

Test 3: Query on persistent connection
bool(true)

Test 4: Second persistent connection (should reuse)
bool(true)

Test 5: Both connections work
bool(true)
bool(true)

Test 6: Close persistent connections
bool(true)
bool(true)

Test 7: Persistent connection with charset
bool(true)

PASS
