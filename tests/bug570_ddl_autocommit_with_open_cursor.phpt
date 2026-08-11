--TEST--
DDL autocommit on default transaction with open cursor from prior SELECT (Issue #570)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * Regression test for GitHub Issue #570:
 *   Cross-process DDL visibility gap when auto_ddl_commit=1
 *
 * The #294 autocommit block in PHP_FUNCTION(fbird_query) commits the
 * default transaction after each non-SELECT statement. Previously,
 * fbt_commit() return value was NOT checked. When commit failed (e.g.,
 * due to open cursors from prior SELECTs on the same default tx),
 * fbt_free() called rollbackNoThrow(), silently losing the DDL.
 *
 * Fix (v13.2.7): fbt_commit() return value is now checked. For DDL,
 * open cursors are closed via _php_fbird_close_tx_cursors() and the
 * commit is retried. For all failures, the error is reported.
 *
 * Test flow:
 *   1. SELECT on default tx (cursor stays open, not freed)
 *   2. CREATE TABLE on default tx (autocommit path: #294 block)
 *      - fbt_commit may fail due to open cursor
 *      - Fix: close cursor, retry commit
 *   3. New connection verifies table is visible (DDL was committed)
 *   4. INSERT on default tx after SELECT (non-DDL autocommit path)
 *   5. New connection verifies data is visible
 */

$conn1 = fbird_connect($test_base);
if (!$conn1) {
    die("FAIL: connect failed: " . fbird_errmsg() . "\n");
}

/* Step 1: SELECT on default transaction.
 * The result is NOT freed — cursor stays open on the default tx.
 * This simulates the doctrine-firebird-driver schema introspection
 * scenario where PHP GC has not yet collected the result resource. */
$result = fbird_query($conn1, 'SELECT COUNT(*) FROM RDB$RELATIONS WHERE RDB$SYSTEM_FLAG = 0');
if (!$result) {
    die("FAIL: initial SELECT failed: " . fbird_errmsg() . "\n");
}
$row = fbird_fetch_row($result);
echo "initial SELECT: " . $row[0] . " user tables\n";
/* Cursor intentionally NOT freed — stays open on default tx */

/* Step 2: DDL on the same connection's default transaction.
 * This goes through the #294 autocommit block in PHP_FUNCTION(fbird_query).
 * The open cursor from Step 1 may cause fbt_commit to fail.
 * Fix (#570): _php_fbird_close_tx_cursors closes the cursor, commit retries.
 *
 * If the fix is not working, this will either:
 * - Throw an exception (in throw mode)
 * - Emit E_WARNING + return false (in warning mode)
 * - Silently lose the DDL (old behavior before fix)
 */
$ddlResult = @fbird_query($conn1, 'CREATE TABLE BUG570_TEST (ID INTEGER)');
if ($ddlResult !== false) {
    echo "DDL autocommit with open cursor: OK\n";
} else {
    echo "DDL autocommit with open cursor: FAILED - " . fbird_errmsg() . "\n";
}

/* Step 3: Open a NEW connection and verify the table is visible.
 * If the DDL commit was silently lost (#570 bug), the table will
 * not exist on the new connection. */
$conn2 = fbird_connect($test_base);
if (!$conn2) {
    die("FAIL: second connect failed: " . fbird_errmsg() . "\n");
}

$checkResult = @fbird_query($conn2, 'SELECT COUNT(*) FROM BUG570_TEST');
if ($checkResult !== false) {
    $checkRow = fbird_fetch_row($checkResult);
    echo "table visible on new connection: OK (rows=" . $checkRow[0] . ")\n";
    fbird_free_result($checkResult);
} else {
    echo "table visible on new connection: FAILED - " . fbird_errmsg() . "\n";
}

/* Step 4: INSERT on default transaction after another SELECT.
 * Tests the non-DDL autocommit path with open cursors. */
$result2 = fbird_query($conn2, 'SELECT 1 FROM RDB$DATABASE');
if ($result2) {
    fbird_fetch_row($result2);
    /* Cursor intentionally NOT freed */
}

$insertResult = @fbird_query($conn2, "INSERT INTO BUG570_TEST (ID) VALUES (42)");
if ($insertResult !== false) {
    echo "INSERT autocommit with open cursor: OK\n";
} else {
    echo "INSERT autocommit with open cursor: FAILED - " . fbird_errmsg() . "\n";
}

/* Step 5: Verify data visible from yet another connection */
$conn3 = fbird_connect($test_base);
if ($conn3) {
    $verifyResult = @fbird_query($conn3, 'SELECT ID FROM BUG570_TEST');
    if ($verifyResult !== false) {
        $verifyRow = fbird_fetch_row($verifyResult);
        echo "data visible on new connection: OK (ID=" . $verifyRow[0] . ")\n";
        fbird_free_result($verifyResult);
    } else {
        echo "data visible on new connection: FAILED - " . fbird_errmsg() . "\n";
    }
    fbird_close($conn3);
}

/* Cleanup: test database is dropped by --CLEAN-- section (clean.inc) */
fbird_close($conn1);
fbird_close($conn2);
echo "Done\n";
?>
--EXPECTF--
initial SELECT: %d user tables
DDL autocommit with open cursor: OK
table visible on new connection: OK (rows=0)
INSERT autocommit with open cursor: OK
data visible on new connection: OK (ID=42)
Done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
