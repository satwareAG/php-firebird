--TEST--
GitHub #135: EXECUTE PROCEDURE tight loop must not leak server-side statements
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
/*
 * Regression test for GitHub issue #135 - EXECUTE PROCEDURE path.
 *
 * EXEC PROCEDURE returns a result snapshot (parent=NULL, owns_stmt_handle=0).
 * The one-shot caller must still free the parent query resource immediately.
 * This test creates a simple stored procedure that returns a value and
 * executes it 200 times in a tight loop via fbird_query().
 */
require("firebird.inc");

$db = fbird_connect($test_base, $user, $password);
if (!$db) die('connect failed');

// Create a simple selectable stored procedure
fbird_query($db, "
    CREATE OR ALTER PROCEDURE SP_ECHO_INT (P_VAL INTEGER)
    RETURNS (R_VAL INTEGER)
    AS
    BEGIN
        R_VAL = P_VAL;
        SUSPEND;
    END
");
fbird_commit($db);

$iterations = 200;
$success = 0;

for ($i = 0; $i < $iterations; $i++) {
    $result = fbird_query($db, "EXECUTE PROCEDURE SP_ECHO_INT($i)");
    if (!$result) {
        echo "FAIL: EXECUTE PROCEDURE returned false at iteration $i\n";
        echo fbird_errmsg() . "\n";
        break;
    }
    $row = fbird_fetch_assoc($result);
    if ($row === false || (int)$row['R_VAL'] !== $i) {
        echo "FAIL: unexpected row at iteration $i\n";
        var_dump($row);
        break;
    }
    fbird_free_result($result);
    $success++;
}

if ($success === $iterations) {
    echo "OK: $iterations EXECUTE PROCEDURE iterations completed without error\n";
}

// Also test via fbird_execute_query with explicit transaction
$trans = fbird_trans($db);
$success2 = 0;
for ($i = 0; $i < $iterations; $i++) {
    $result = fbird_execute_query($trans, "EXECUTE PROCEDURE SP_ECHO_INT($i)");
    if (!$result) {
        echo "FAIL: fbird_execute_query EXEC PROC returned false at iteration $i\n";
        echo fbird_errmsg() . "\n";
        break;
    }
    $row = fbird_fetch_assoc($result);
    if ($row === false || (int)$row['R_VAL'] !== $i) {
        echo "FAIL: unexpected row at iteration $i (execute_query path)\n";
        var_dump($row);
        break;
    }
    fbird_free_result($result);
    $success2++;
}
fbird_commit($trans);

if ($success2 === $iterations) {
    echo "OK: $iterations fbird_execute_query EXEC PROC iterations completed\n";
}

// Cleanup
fbird_close($db);
$db2 = fbird_connect($test_base, $user, $password);
fbird_query($db2, "DROP PROCEDURE SP_ECHO_INT");
fbird_commit($db2);
fbird_close($db2);
?>
--EXPECT--
OK: 200 EXECUTE PROCEDURE iterations completed without error
OK: 200 fbird_execute_query EXEC PROC iterations completed
