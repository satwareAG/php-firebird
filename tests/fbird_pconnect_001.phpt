--TEST--
fbird_pconnect() - persistent connection test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

echo "=== fbird_pconnect() test ===\n";

// Test 1: Basic persistent connection
echo "Test 1: Basic pconnect\n";
$db1 = fbird_pconnect($test_base, $user, $password);
var_dump($db1 !== false);

// Test 2: Verify INI settings exist
echo "\nTest 2: INI settings\n";
var_dump(ini_get('fbird.allow_persistent') !== false);
var_dump(ini_get('fbird.max_persistent') !== false);
var_dump(ini_get('fbird.max_links') !== false);

// Test 3: Second persistent connection (may reuse)
echo "\nTest 3: Second pconnect\n";
$db2 = fbird_pconnect($test_base, $user, $password);
var_dump($db2 !== false);

// Test 4: Basic query on persistent connection
echo "\nTest 4: Query on pconnect\n";
$result = fbird_query($db1, "SELECT 1 FROM RDB\$DATABASE");
var_dump($result !== false);
if ($result) {
    $row = fbird_fetch_row($result);
    var_dump($row[0] == 1);
    fbird_free_result($result);
}

// Cleanup
@fbird_close($db1);
@fbird_close($db2);

echo "\nPASS\n";
?>
--EXPECT--
=== fbird_pconnect() test ===
Test 1: Basic pconnect
bool(true)

Test 2: INI settings
bool(true)
bool(true)
bool(true)

Test 3: Second pconnect
bool(true)

Test 4: Query on pconnect
bool(true)
bool(true)

PASS
