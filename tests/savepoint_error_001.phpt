--TEST--
fbird_savepoint() error handling
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

$db = fbird_connect($test_base);
$trans = fbird_trans($db);

// Test 1: Invalid savepoint name (empty)
fbird_savepoint($trans, "");

// Test 2: Rollback to unknown savepoint
// This usually triggers a Firebird error
if (!fbird_rollback_savepoint($trans, "NON_EXISTENT_SV")) {
    // Expecting generic error handler to catch it or return false
    echo "Rollback failed as expected\n";
}

// Test 3: Invalid release (unknown)
if (!fbird_release_savepoint($trans, "NON_EXISTENT_SV")) {
    echo "Release failed as expected\n";
}

fbird_commit($trans);
fbird_close($db);

?>
--EXPECTF--
Warning: fbird_savepoint(): Invalid savepoint name (length must be 1-31 bytes) in %s on line %d

Warning: fbird_rollback_savepoint(): %s in %s on line %d
Rollback failed as expected

Warning: fbird_release_savepoint(): %s in %s on line %d
Release failed as expected
