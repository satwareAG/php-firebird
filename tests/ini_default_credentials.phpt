--TEST--
fbird.default_* INI settings (user, password, db, charset)
--EXTENSIONS--
firebird
--SKIPIF--
<?php require_once 'skipif.inc'; ?>
--INI--
fbird.default_user=SYSDBA
fbird.default_password=masterkey
fbird.default_charset=UTF8
--FILE--
<?php
require_once 'firebird.inc';

echo "=== fbird.default_* INI test ===\n";

// Test 1: Check INI values are set
echo "Test 1: Check INI values\n";
var_dump(ini_get('fbird.default_user'));
var_dump(ini_get('fbird.default_password'));
var_dump(ini_get('fbird.default_db'));
var_dump(ini_get('fbird.default_charset'));

// Test 2: Connect with only database path (using defaults)
echo "\nTest 2: Connect using default credentials\n";
// Use the default user/password from INI
$db = fbird_connect(TEST_DB_PATH);
var_dump(is_resource($db) || $db !== false);

// Test 3: Verify connection works
echo "\nTest 3: Verify connection works\n";
$result = fbird_query($db, "SELECT CURRENT_USER FROM RDB\$DATABASE");
$row = fbird_fetch_row($result);
echo "Connected as: " . (strlen($row[0]) > 0 ? "user found" : "no user") . "\n";
fbird_free_result($result);
fbird_close($db);

// Test 4: Override default user with explicit parameter
echo "\nTest 4: Override with explicit credentials\n";
$db2 = fbird_connect(TEST_DB_PATH, TEST_USER, TEST_PASS);
var_dump(is_resource($db2) || $db2 !== false);
fbird_close($db2);

// Test 5: Charset from INI
echo "\nTest 5: Default charset from INI\n";
var_dump(ini_get('fbird.default_charset') === 'UTF8');

echo "\nPASS\n";
?>
--EXPECT--
=== fbird.default_* INI test ===
Test 1: Check INI values
string(6) "SYSDBA"
string(9) "masterkey"
string(0) ""
string(4) "UTF8"

Test 2: Connect using default credentials
bool(true)

Test 3: Verify connection works
Connected as: user found

Test 4: Override with explicit credentials
bool(true)

Test 5: Default charset from INI
bool(true)

PASS
