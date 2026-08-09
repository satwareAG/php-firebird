--TEST--
fbird_release_metadata_locks() explicit API (#566)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * Issue #566: fbird_release_metadata_locks() — explicit metadata lock release.
 *
 * This API hard-commits the transaction (releasing all locks including
 * metadata from prior cursor activity) and restarts it with the original TPB.
 * The transaction handle stays valid for the caller.
 *
 * Use case: When you have open cursors from schema introspection and want
 * to do DDL without the transparent commit+restart, you can explicitly
 * release locks first.
 */

$conn = fbird_connect($test_base);

/* Step 1: Start explicit transaction */
$tx = fbird_trans($conn);
echo "transaction started\n";

/* Step 2: Open a cursor (schema introspection) */
$result = fbird_query($tx, "SELECT COUNT(*) FROM RDB\$RELATIONS");
$row = fbird_fetch_row($result);
echo "introspection: {$row[0]} tables\n";
fbird_free_result($result);

/* Step 3: Explicitly release metadata locks.
 * This commits + restarts the transaction. After this, DDL should work
 * without any transparent commit+restart. */
$released = fbird_release_metadata_locks($tx);
echo "release_metadata_locks: " . ($released ? "OK" : "FAILED") . "\n";

/* Step 4: Verify transaction is still active (restart was transparent) */
if (fbird_query($tx, "SELECT 1 FROM RDB\$DATABASE") !== false) {
    $row = fbird_fetch_row(fbird_query($tx, "SELECT 1 FROM RDB\$DATABASE"));
    echo "transaction still active: yes\n";
} else {
    echo "transaction still active: no\n";
}

/* Step 5: DDL should work after explicit release */
$ddl = fbird_query($tx, "CREATE TABLE ISSUE566_TEST (id INTEGER)");
echo "DDL after release: " . ($ddl !== false ? "OK" : "FAILED") . "\n";

/* Cleanup */
fbird_query($tx, "DROP TABLE ISSUE566_TEST");
fbird_commit($tx);
fbird_close($conn);
echo "Done\n";
?>
--EXPECTF--
transaction started
introspection: %d tables
release_metadata_locks: OK
transaction still active: yes
DDL after release: OK
Done

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
