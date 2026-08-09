--TEST--
DDL without open cursors preserves transactional atomicity (#566)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * Issue #566: DDL on explicit transaction should NOT auto-commit when
 * no open cursors hold metadata locks.
 *
 * This is the doctrine-firebird-driver TemporaryTableTest scenario:
 *   1. beginTransaction → starts T1
 *   2. INSERT id=1 → DML (no cursor opened)
 *   3. DROP/CREATE temp table → DDL with cursor_count=0 → NO commit+restart
 *   4. INSERT id=2 → DML in T1
 *   5. rollBack → undoes everything in T1
 *   6. SELECT → empty (all rolled back)
 *
 * In v13.1.0, step 3 triggered transparent commit+restart, which committed
 * id=1. rollBack only undid id=2. This test verifies the #566 fix.
 */

$conn = fbird_connect($test_base);

/* Create a base table for the test */
$setup_tx = fbird_trans($conn);
fbird_query($setup_tx, "CREATE TABLE NONTEMPORARY (id INTEGER)");
fbird_commit($setup_tx);

/* The actual test */
$tx = fbird_trans($conn);

/* INSERT id=1 — DML, no cursor */
fbird_query($tx, "INSERT INTO NONTEMPORARY (id) VALUES (1)");

/* DDL — DROP a non-existent temp table (error ignored), then CREATE.
 * cursor_count = 0 → NO transparent commit+restart (#566). */
@fbird_query($tx, "DROP TABLE MY_TEMPORARY");
fbird_query($tx, "CREATE TABLE MY_TEMPORARY (id INTEGER)");

/* INSERT id=2 — still in T1 */
fbird_query($tx, "INSERT INTO NONTEMPORARY (id) VALUES (2)");

/* Rollback should undo BOTH inserts */
fbird_rollback($tx);

/* Verify: table should be empty */
$verify_tx = fbird_trans($conn);
$result = fbird_query($verify_tx, "SELECT COUNT(*) FROM NONTEMPORARY");
$row = fbird_fetch_row($result);
$count = (int)$row[0];
fbird_free_result($result);
fbird_commit($verify_tx);

if ($count === 0) {
    echo "PASS: Transactional DDL preserved - rollback undid all changes\n";
} else {
    echo "FAIL: Data leaked after rollback - found $count rows (expected 0)\n";
    echo "This means the DDL triggered transparent commit (#540 regression)\n";
}

/* Cleanup */
$cleanup_tx = fbird_trans($conn);
@fbird_query($cleanup_tx, "DROP TABLE MY_TEMPORARY");
fbird_query($cleanup_tx, "DROP TABLE NONTEMPORARY");
fbird_commit($cleanup_tx);

fbird_close($conn);
echo "Done\n";
?>
--EXPECT--
PASS: Transactional DDL preserved - rollback undid all changes
Done

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
