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

    $dsn = dirname($test_base) . "/issue121_" . getmypid() . ".fdb";

    $conn = fbird_create_database($dsn, $user, $password, 'UTF8', 8192);

    if ($conn) {
        echo "OK: Database created\n";
        fbird_drop_db($conn);
        echo "OK: Database dropped\n";
    } else {
        echo "FAIL: Could not create database: " . fbird_errmsg() . "\n";
    }
} else {
    echo "FAIL: fbird_create_database does not exist\n";
}

?>
--EXPECTF--
Checking if fbird_create_database exists...
OK: fbird_create_database exists
OK: Database created
OK: Database dropped
