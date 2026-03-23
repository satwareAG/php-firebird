--TEST--
Issue #125: Deprecation of FBIRD_CREATE in fbird_query()
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

echo "Checking FBIRD_CREATE deprecation in fbird_query()...\n";
$dsn = dirname($test_base) . "/issue125_" . getmypid() . ".fdb";
$sql = sprintf("CREATE DATABASE '%s' USER '%s' PASSWORD '%s'", $dsn, $user, $password);

/* This should emit E_DEPRECATED but still work */
$conn = fbird_query(FBIRD_CREATE, $sql);

if ($conn) {
    echo "OK: Database created via FBIRD_CREATE\n";
    fbird_drop_db($conn);
    echo "OK: dropped\n";
} else {
    echo "FAIL: Could not create database via FBIRD_CREATE\n";
}

?>
--EXPECTF--
Checking FBIRD_CREATE deprecation in fbird_query()...

Deprecated: fbird_query(): Passing FBIRD_CREATE to fbird_query() is deprecated, use fbird_create_database() instead in %s on line %d
OK: Database created via FBIRD_CREATE
OK: dropped
