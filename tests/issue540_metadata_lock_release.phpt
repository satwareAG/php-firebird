--TEST--
DDL after schema introspection releases metadata locks via cursor counter (Issue #540/#566)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * Regression test for GitHub Issues #540 + #566:
 *   Metadata lock release for explicit transactions
 *
 * Root cause (#540):
 *   Firebird holds metadata locks at the transaction level. SELECT cursors
 *   from schema introspection hold shared metadata locks on system tables.
 *   When DDL is attempted on the same transaction, it blocks because the
 *   metadata locks conflict.
 *
 * Fix (v13.2.0, #566 refinement):
 *   The transparent commit+restart now fires ONLY when open_cursor_count > 0,
 *   i.e., when there are unfreed SELECT cursors holding locks. This preserves
 *   transactional DDL semantics (DDL after DML without open cursors stays
 *   atomic) while still fixing the deadlock scenario.
 *
 * Additionally, fbird_release_metadata_locks() provides an explicit API
 * for callers who want manual control over lock release.
 *
 * Test flow:
 *   1. conn1: explicit tx, SELECT from RDB$RELATIONS (cursor stays open)
 *   2. conn1: CREATE TABLE → cursor_count > 0 → transparent commit+restart
 *   3. conn1: commit tx
 *   4. conn2: DROP TABLE from another connection (must not block)
 *
 * Note: The cursor is NOT freed before DDL — this matches the real-world
 * doctrine scenario where PHP GC hasn't collected result objects yet.
 */

$conn1 = fbird_connect($test_base);
$conn2 = fbird_connect($test_base);

/* Step 1: Schema introspection in explicit transaction.
 * Do NOT free the result — cursor stays open (cursor_count = 1). */
$tx = fbird_trans($conn1);
$result = fbird_query($tx, "SELECT COUNT(*) FROM RDB\$RELATIONS WHERE RDB\$SYSTEM_FLAG = 0");
$row = fbird_fetch_row($result);
echo "introspection: " . $row[0] . " user tables\n";
/* Cursor intentionally NOT freed: simulates PHP GC delay in doctrine */

/*
 * Step 2: DDL in the same transaction.
 * open_cursor_count > 0 → transparent commit+restart fires.
 * The commit releases all metadata locks, the restart starts a fresh tx.
 * Note: DDL returns true (not a resource) since it has no result set.
 */
$ddlResult = fbird_query($tx, "CREATE TABLE ISSUE540_TEST (id INTEGER)");
if ($ddlResult !== false) {
    echo "DDL same tx with open cursor: OK\n";
} else {
    echo "DDL same tx with open cursor: FAILED - " . fbird_errmsg() . "\n";
}

/* Step 3: commit tx so conn2 can see the new table */
fbird_commit($tx);

/* Step 4: Another connection drops the table (must not block) */
$tx2 = fbird_trans($conn2);
$dropResult = @fbird_query($tx2, "DROP TABLE ISSUE540_TEST");
if ($dropResult !== false) {
    fbird_commit($tx2);
    echo "DDL cross-connection: OK\n";
} else {
    fbird_rollback($tx2);
    echo "DDL cross-connection: " . fbird_errmsg() . "\n";
}

fbird_close($conn1);
fbird_close($conn2);
echo "Done\n";
?>
--EXPECT--
introspection: 1 user tables
DDL same tx with open cursor: OK
DDL cross-connection: OK
Done

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
