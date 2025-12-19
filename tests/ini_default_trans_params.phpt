--TEST--
fbird.default_trans_params INI setting
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

echo "=== fbird.default_trans_params INI setting test ===\n";

// Test 1: Check INI setting exists
echo "Test 1: INI setting exists\n";
var_dump(ini_get('fbird.default_trans_params') !== false);

// Test 2: Get default value
echo "\nTest 2: Default value\n";
$default = ini_get('fbird.default_trans_params');
echo "Default trans_params: $default\n";

// Test 3: Set new value (Note: uses numeric value, string name just stored as-is)
echo "\nTest 3: Set custom value\n";
@ini_set('fbird.default_trans_params', 'FBIRD_READ');
$new_value = ini_get('fbird.default_trans_params');
echo "New value: $new_value\n";
var_dump($new_value === 'FBIRD_READ');

// Test 4: Basic transaction with default settings
echo "\nTest 4: Transaction operations\n";
$db = fbird_connect($test_base, $user, $password);
var_dump($db !== false);

if ($db) {
    // Start a transaction
    $trans = fbird_trans($db);
    var_dump($trans !== false);

    if ($trans) {
        // Simple query within transaction
        $result = fbird_query($trans, "SELECT 1 FROM RDB\$DATABASE");
        var_dump($result !== false);
        if ($result) fbird_free_result($result);

        // Commit transaction
        var_dump(fbird_commit($trans));
    }

    fbird_close($db);
}

// Restore default
ini_set('fbird.default_trans_params', $default);

echo "\nPASS\n";
?>
--EXPECT--
=== fbird.default_trans_params INI setting test ===
Test 1: INI setting exists
bool(true)

Test 2: Default value
Default trans_params: 0

Test 3: Set custom value
New value: FBIRD_READ
bool(true)

Test 4: Transaction operations
bool(true)
bool(true)
bool(true)
bool(true)

PASS
