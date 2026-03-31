--TEST--
GitHub #135: fbird_query_params_tx() SELECT must free parent statement immediately (no server leak)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
/*
 * Regression test for GitHub issue #135 - fbird_query_params_tx() path.
 *
 * fbird_query_params_tx() is a one-shot prepare+execute function for
 * parameterized queries with explicit link AND transaction handles.
 * It must transfer statement ownership from the internal parent to the result
 * resource and free the parent immediately for result-returning queries.
 *
 * This test runs 200 iterations with a parameterized SELECT to verify
 * no statement accumulation.
 */
require("firebird.inc");

$db = fbird_connect($test_base, $user, $password);
if (!$db) die('connect failed');

$trans = fbird_trans($db);
if (!$trans) die('trans failed');

$iterations = 200;
$success = 0;

for ($i = 0; $i < $iterations; $i++) {
    $result = fbird_query_params_tx($db, $trans, "SELECT CAST(? AS INTEGER) AS VAL FROM RDB\$DATABASE", [$i]);
    if (!$result) {
        echo "FAIL: fbird_query_params_tx returned false at iteration $i\n";
        echo fbird_errmsg() . "\n";
        break;
    }
    $row = fbird_fetch_assoc($result);
    if ($row === false || (int)$row['VAL'] !== $i) {
        echo "FAIL: unexpected row at iteration $i\n";
        var_dump($row);
        break;
    }
    fbird_free_result($result);
    $success++;
}

if ($success === $iterations) {
    echo "OK: $iterations iterations completed without error\n";
}

fbird_commit($trans);
fbird_close($db);
?>
--EXPECT--
OK: 200 iterations completed without error
