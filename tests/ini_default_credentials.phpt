--TEST--
fbird.default_* INI settings (user, password, db, charset)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

echo "=== fbird.default_* INI settings test ===\n";

// Test 1: Check default user/password settings existence
echo "Test 1: INI settings existence\n";
var_dump(ini_get('fbird.default_user') !== false);
var_dump(ini_get('fbird.default_password') !== false);
var_dump(ini_get('fbird.default_db') !== false);
var_dump(ini_get('fbird.default_charset') !== false);

// Test 2: Get current values
echo "\nTest 2: Current values\n";
$default_user = ini_get('fbird.default_user');
$default_password = ini_get('fbird.default_password');
$default_db = ini_get('fbird.default_db');
$default_charset = ini_get('fbird.default_charset');

echo "default_user: " . (strlen($default_user) > 0 ? "set" : "empty") . "\n";
echo "default_password: " . (strlen($default_password) > 0 ? "set" : "empty") . "\n";
echo "default_db: " . (strlen($default_db) > 0 ? "set" : "empty") . "\n";
echo "default_charset: " . (strlen($default_charset) > 0 ? "set" : "empty") . "\n";

// Test 3: Verify basic connection still works with explicit params
echo "\nTest 3: Connection with explicit params\n";
$db = fbird_connect($test_base, $user, $password);
var_dump($db !== false);

if ($db) {
    $result = fbird_query($db, "SELECT 1 FROM RDB\$DATABASE");
    var_dump($result !== false);
    if ($result) fbird_free_result($result);
    fbird_close($db);
}

echo "\nPASS\n";
?>
--EXPECT--
=== fbird.default_* INI settings test ===
Test 1: INI settings existence
bool(true)
bool(true)
bool(true)
bool(true)

Test 2: Current values
default_user: empty
default_password: empty
default_db: empty
default_charset: empty

Test 3: Connection with explicit params
bool(true)
bool(true)

PASS
