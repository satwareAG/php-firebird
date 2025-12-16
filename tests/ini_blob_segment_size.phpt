--TEST--
fbird.blob_segment_size INI setting
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

echo "=== fbird.blob_segment_size INI setting test ===\n";

// Test 1: Check INI setting exists
echo "Test 1: INI setting exists\n";
var_dump(ini_get('fbird.blob_segment_size') !== false);

// Test 2: Get default value
echo "\nTest 2: Default value\n";
$default = ini_get('fbird.blob_segment_size');
echo "Default blob_segment_size: $default\n";
var_dump(is_numeric($default));

// Test 3: Set new value
echo "\nTest 3: Set custom value\n";
ini_set('fbird.blob_segment_size', '8192');
$new_value = ini_get('fbird.blob_segment_size');
echo "New value: $new_value\n";
var_dump($new_value == '8192');

// Test 4: Test blob operations with custom segment size
echo "\nTest 4: Blob operations\n";
$db = fbird_connect($test_base, $user, $password);
var_dump($db !== false);

if ($db) {
    // Create test data larger than segment size
    $test_data = str_repeat("X", 10000);

    // Create blob
    $blob_id = fbird_blob_create($db);
    var_dump($blob_id !== false);

    if ($blob_id) {
        fbird_blob_add($blob_id, $test_data);
        $blob_handle = fbird_blob_close($blob_id);
        var_dump($blob_handle !== false);
    }

    fbird_close($db);
}

// Restore default
ini_set('fbird.blob_segment_size', $default);

echo "\nPASS\n";
?>
--EXPECT--
=== fbird.blob_segment_size INI setting test ===
Test 1: INI setting exists
bool(true)

Test 2: Default value
Default blob_segment_size: 4096
bool(true)

Test 3: Set custom value
New value: 8192
bool(true)

Test 4: Blob operations
bool(true)
bool(true)
bool(true)

PASS
