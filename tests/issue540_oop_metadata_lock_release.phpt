--TEST--
DDL via OOP prepare+execute does not regress after #540 _php_fbird_exec fix
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * OOP variant of issue540_metadata_lock_release.phpt
 *
 * The OOP API (Firebird\Connection::prepare + Statement::execute)
 * calls the same _php_fbird_exec() as the procedural path, so the
 * transparent DDL commit+restart fix is active for OOP too.
 *
 * The OOP API does not have commitRetaining(), so the metadata lock
 * scenario from #540 is procedural-only. This test verifies DDL
 * through the OOP prepare+execute path works correctly and that
 * the fix in _php_fbird_exec() does not break OOP DDL.
 *
 * Flow:
 *   1. OOP connection + beginTransaction
 *   2. OOP prepare + execute DDL (CREATE TABLE)
 *   3. commit
 *   4. Procedural cross-connection DROP TABLE (verifies visibility)
 */

$conn = fbird_connect($test_base);

/* Step 1: OOP connection + transaction */
$oop_conn = new \Firebird\Connection($test_base, $user, $password);
$oop_tx = $oop_conn->beginTransaction();

/* Step 2: DDL via OOP prepare + execute */
$stmt = $oop_conn->prepare("CREATE TABLE ISSUE540_OOP_TEST (id INTEGER)", $oop_tx);
$stmt->execute($oop_tx);
echo "DDL via OOP prepare+execute: OK\n";

/* Step 3: commit so the table is visible to other connections */
$oop_tx->commit();

/* Step 4: Cross-connection DROP (procedural, separate attachment to same DB) */
$tx2 = fbird_trans($conn);
$dropResult = @fbird_query($tx2, "DROP TABLE ISSUE540_OOP_TEST");
if ($dropResult !== false) {
    fbird_commit($tx2);
    echo "Cross-connection DROP: OK\n";
} else {
    fbird_rollback($tx2);
    echo "Cross-connection DROP: " . fbird_errmsg() . "\n";
}

/* Cleanup */
$oop_conn->close();
fbird_close($conn);
echo "Done\n";
?>
--EXPECT--
DDL via OOP prepare+execute: OK
Cross-connection DROP: OK
Done

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
