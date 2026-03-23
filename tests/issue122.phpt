--TEST--
Issue #122: fbird_drop_db() without open resource
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

echo "Checking if fbird_drop_database exists (procedural with connection string)...\n";
if (function_exists('fbird_drop_database')) {
    echo "OK: fbird_drop_database exists\n";
    // Create DB first
    $dbPath = $test_base . "_i122";
    $sql = sprintf("CREATE DATABASE '%s' USER '%s' PASSWORD '%s'", $dbPath, $user, $password);
    fbird_query(FBIRD_CREATE, $sql);
    
    // Now drop it using connection string
    $result = fbird_drop_database($dbPath, $user, $password);
    if ($result) {
        echo "OK: Database dropped via connection string\n";
    } else {
        echo "FAIL: Could not drop database: " . fbird_errmsg() . "\n";
    }
} else {
    echo "Current behavior: fbird_drop_database does not exist\n";
    
    // Create DB first
    $dbPath = $test_base . "_i122";
    $sql = sprintf("CREATE DATABASE '%s' USER '%s' PASSWORD '%s'", $dbPath, $user, $password);
    fbird_query(FBIRD_CREATE, $sql);
    
    // Drop using resource
    $conn = fbird_connect($dbPath, $user, $password);
    $result = fbird_drop_db($conn);
    if ($result) {
        echo "OK: Database dropped via resource\n";
    } else {
        echo "FAIL: Could not drop database: " . fbird_errmsg() . "\n";
    }
}
?>
--EXPECTF--
Checking if fbird_drop_database exists (procedural with connection string)...
Current behavior: fbird_drop_database does not exist
OK: Database dropped via resource
