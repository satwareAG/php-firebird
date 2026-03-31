--TEST--
GitHub #135: DML RETURNING tight loop must not leak server-side statements (FB 2.1+)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
/*
 * Regression test for GitHub issue #135 - DML RETURNING path.
 *
 * INSERT ... RETURNING produces a result resource via the EXEC PROCEDURE
 * code path (singleton result snapshot). The one-shot caller must free the
 * parent query resource immediately after returning the result to userland.
 *
 * DML RETURNING has been supported since Firebird 2.1, so no version skip.
 * This test runs 200 iterations of INSERT RETURNING via fbird_query().
 */
require("firebird.inc");

$db = fbird_connect($test_base, $user, $password);
if (!$db) die('connect failed');

// Create test table
fbird_query($db, "CREATE TABLE gh135_dml_ret (id INTEGER NOT NULL, val VARCHAR(32))");
fbird_commit($db);

$iterations = 200;
$success = 0;

for ($i = 0; $i < $iterations; $i++) {
    $result = fbird_query($db, "INSERT INTO gh135_dml_ret (id, val) VALUES ($i, 'row_$i') RETURNING id, val");
    if (!$result) {
        echo "FAIL: INSERT RETURNING returned false at iteration $i\n";
        echo fbird_errmsg() . "\n";
        break;
    }
    $row = fbird_fetch_assoc($result);
    if ($row === false || (int)$row['ID'] !== $i) {
        echo "FAIL: unexpected row at iteration $i\n";
        var_dump($row);
        break;
    }
    fbird_free_result($result);
    $success++;
}
fbird_commit($db);

if ($success === $iterations) {
    echo "OK: $iterations INSERT RETURNING iterations completed without error\n";
}

// Verify row count
$result = fbird_query($db, "SELECT COUNT(*) AS CNT FROM gh135_dml_ret");
$row = fbird_fetch_assoc($result);
fbird_free_result($result);

if ((int)$row['CNT'] === $iterations) {
    echo "OK: all $iterations rows present\n";
} else {
    echo "FAIL: expected $iterations rows, got {$row['CNT']}\n";
}

// Also test UPDATE RETURNING tight loop
$success2 = 0;
for ($i = 0; $i < $iterations; $i++) {
    $result = fbird_query($db, "UPDATE gh135_dml_ret SET val = 'upd_$i' WHERE id = $i RETURNING id");
    if (!$result) {
        echo "FAIL: UPDATE RETURNING returned false at iteration $i\n";
        echo fbird_errmsg() . "\n";
        break;
    }
    $row = fbird_fetch_assoc($result);
    if ($row === false || (int)$row['ID'] !== $i) {
        echo "FAIL: unexpected UPDATE RETURNING row at iteration $i\n";
        var_dump($row);
        break;
    }
    fbird_free_result($result);
    $success2++;
}
fbird_commit($db);

if ($success2 === $iterations) {
    echo "OK: $iterations UPDATE RETURNING iterations completed without error\n";
}

// Cleanup
fbird_close($db);
$db2 = fbird_connect($test_base, $user, $password);
fbird_query($db2, "DROP TABLE gh135_dml_ret");
fbird_commit($db2);
fbird_close($db2);
?>
--EXPECT--
OK: 200 INSERT RETURNING iterations completed without error
OK: all 200 rows present
OK: 200 UPDATE RETURNING iterations completed without error
