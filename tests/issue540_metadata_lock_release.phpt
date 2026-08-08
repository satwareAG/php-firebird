--TEST--
DDL after fbird_commit_ret() releases metadata locks from prior SELECT (Issue #540)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * Regression test for GitHub Issue #540:
 *   Metadata lock release API for auto-commit mode
 *   (fbird_commit_ret holds locks, blocking DDL after introspection)
 *
 * Root cause:
 *   Firebird holds metadata locks at the transaction level. fbird_commit_ret()
 *   commits data changes but retains the transaction context (snapshot
 *   preserved). In the doctrine-firebird-driver auto-commit simulation,
 *   SELECT cursors from schema introspection hold metadata locks that
 *   persist across commit_ret, blocking subsequent DDL operations.
 *
 * Fix (v13.1.0):
 *   When fbird_query() detects a DDL statement
 *   (statement_type == isc_info_sql_stmt_ddl) on a transaction that has had
 *   prior cursor activity, it transparently does a hard commit + restart
 *   before executing the DDL. This releases all metadata locks while keeping
 *   the same transaction handle valid for the caller.
 *
 * Test flow:
 *   1. conn1: explicit tx, SELECT from RDB$RELATIONS (acquires metadata lock)
 *   2. conn1: fbird_commit_ret() (retains transaction)
 *   3. conn1: CREATE TABLE in same tx (fix does transparent commit+restart)
 *   4. conn1: commit tx
 *   5. conn2: DROP TABLE from another connection (must not block)
 *
 * Note: Requires FIREBIRD_DB_DIR (per-test unique DB) so the user table
 * count is deterministic (init_db() creates exactly 1 table: test1).
 *
 * Before fix: step 3 hangs (metadata lock held by retained tx)
 * After fix:  all steps succeed
 */

$conn1 = fbird_connect($test_base);
$conn2 = fbird_connect($test_base);

/* Step 1: Schema introspection in explicit transaction */
$tx = fbird_trans($conn1);
$result = fbird_query($tx, "SELECT COUNT(*) FROM RDB\$RELATIONS WHERE RDB\$SYSTEM_FLAG = 0");
$row = fbird_fetch_row($result);
echo "introspection: " . $row[0] . " user tables\n";
fbird_free_result($result);

/* Step 2: commit_ret - retains the transaction context */
fbird_commit_ret($tx);
echo "commit_ret ok\n";

/*
 * Step 3: DDL in the same transaction.
 * After commit_ret, metadata locks from the SELECT should be released.
 * The fix auto-detects DDL and does a transparent hard commit+restart
 * before executing the DDL statement.
 * Note: DDL returns true (not a resource) since it has no result set.
 */
$ddlResult = fbird_query($tx, "CREATE TABLE ISSUE540_TEST (id INTEGER)");
if ($ddlResult !== false) {
    echo "DDL same tx after commit_ret: OK\n";
} else {
    echo "DDL same tx after commit_ret: FAILED - " . fbird_errmsg() . "\n";
}

/* Step 4: commit tx so conn2 can see the new table */
fbird_commit($tx);

/* Step 5: Another connection drops the table (must not block) */
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
commit_ret ok
DDL same tx after commit_ret: OK
DDL cross-connection: OK
Done

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
