--TEST--
fbird_connection_info() - basic functionality
--EXTENSIONS--
firebird
--SKIPIF--
<?php
include("skipif.inc");
?>
--FILE--
<?php

require("firebird.inc");

$db = fbird_connect($test_base);
if (!$db) {
    die("Failed to connect: " . fbird_errmsg());
}

// Test basic call
$info = fbird_connection_info($db);
var_dump(is_array($info));

// Verify expected keys exist
$expected_keys = [
    'reads',
    'writes',
    'fetches',
    'marks',
    'page_size',
    'num_buffers',
    'current_memory',
    'max_memory',
    'allocation',
    'attachment_id',
    'ods_version',
    'ods_minor_version',
    'sql_dialect'
];

$missing = [];
foreach ($expected_keys as $key) {
    if (!array_key_exists($key, $info)) {
        $missing[] = $key;
    }
}

if (count($missing) > 0) {
    echo "Missing keys: " . implode(', ', $missing) . "\n";
} else {
    echo "All expected keys present\n";
}

// Verify values are integers
$all_integers = true;
foreach ($info as $key => $value) {
    if (!is_int($value)) {
        echo "Non-integer value for key: $key\n";
        $all_integers = false;
    }
}
if ($all_integers) {
    echo "All values are integers\n";
}

// Verify some sensible values
echo "page_size > 0: " . ($info['page_size'] > 0 ? "YES" : "NO") . "\n";
echo "ods_version > 0: " . ($info['ods_version'] > 0 ? "YES" : "NO") . "\n";
echo "sql_dialect valid: " . (in_array($info['sql_dialect'], [1, 2, 3]) ? "YES" : "NO") . "\n";
echo "attachment_id > 0: " . ($info['attachment_id'] > 0 ? "YES" : "NO") . "\n";

// Test with default connection (no parameter)
$info2 = fbird_connection_info();
var_dump(is_array($info2));

fbird_close($db);

// Test with closed connection (should fail with TypeError in PHP 8+)
try {
    $info3 = fbird_connection_info($db);
    var_dump($info3);
} catch (TypeError $e) {
    echo "TypeError caught (expected)\n";
}

echo "Done\n";
?>
--EXPECT--
bool(true)
All expected keys present
All values are integers
page_size > 0: YES
ods_version > 0: YES
sql_dialect valid: YES
attachment_id > 0: YES
bool(true)
TypeError caught (expected)
Done
