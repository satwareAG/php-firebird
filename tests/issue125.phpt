--TEST--
Issue #125: Deprecation of FBIRD_CREATE in fbird_query()
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

echo "Checking if FBIRD_CREATE is still supported in fbird_query()...\n";
$dbPath = $test_base . "_i125";
$sql = sprintf("CREATE DATABASE '%s' USER '%s' PASSWORD '%s'", $dbPath, $user, $password);

// This should currently work but we want to know if it emits a warning (not yet)
$conn = fbird_query(FBIRD_CREATE, $sql);

if ($conn) {
    echo "OK: Database created via FBIRD_CREATE\n";
    fbird_drop_db($conn);
} else {
    echo "FAIL: Could not create database via FBIRD_CREATE\n";
}

?>
--EXPECTF--
Checking if FBIRD_CREATE is still supported in fbird_query()...
OK: Database created via FBIRD_CREATE
