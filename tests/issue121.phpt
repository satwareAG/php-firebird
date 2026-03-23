--TEST--
Issue #121: add fbird_create_database() as a first-class procedural function
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

echo "Checking if fbird_create_database exists...\n";
if (function_exists('fbird_create_database')) {
    echo "OK: fbird_create_database exists\n";
    
    // Test it (this would fail if not implemented)
    $dbPath = dirname($test_base) . "/issue121.fdb";
    @unlink($dbPath);
    
    $conn = fbird_create_database($dbPath, $user, $password, [
        'page_size' => 8192,
        'charset' => 'UTF8'
    ]);
    
    if ($conn) {
        echo "OK: Database created\n";
        fbird_drop_db($conn);
    } else {
        echo "FAIL: Could not create database: " . fbird_errmsg() . "\n";
    }
} else {
    echo "Current behavior: fbird_create_database does not exist\n";
    
    // Current alternative is using FBIRD_CREATE constant
    $sql = sprintf("CREATE DATABASE '%s' USER '%s' PASSWORD '%s'", $test_base . "_i121", $user, $password);
    $conn = fbird_query(FBIRD_CREATE, $sql);
    if ($conn) {
        echo "Alternative worked: FBIRD_CREATE still supported\n";
        fbird_drop_db($conn);
    }
}

?>
--EXPECTF--
Checking if fbird_create_database exists...
Current behavior: fbird_create_database does not exist
Alternative worked: FBIRD_CREATE still supported
