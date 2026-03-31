--TEST--
GitHub #135: fbird_execute_query() SELECT must free parent statement immediately (no server leak)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
/*
 * Regression test for GitHub issue #135 - fbird_execute_query() path.
 *
 * fbird_execute_query() is a one-shot prepare+execute function for SELECT
 * statements that takes an explicit transaction handle. Like fbird_query(),
 * it must transfer statement ownership from the internal parent to the result
 * resource and free the parent immediately.
 *
 * This test runs 200 iterations to verify no statement accumulation.
 */
require("firebird.inc");

$db = fbird_connect($test_base, $user, $password);
if (!$db) die('connect failed');

$trans = fbird_trans($db);
if (!$trans) die('trans failed');

$iterations = 200;
$success = 0;

for ($i = 0; $i < $iterations; $i++) {
    $result = fbird_execute_query($trans, "SELECT 1 AS VAL FROM RDB\$DATABASE");
    if (!$result) {
        echo "FAIL: fbird_execute_query returned false at iteration $i\n";
        echo fbird_errmsg() . "\n";
        break;
    }
    $row = fbird_fetch_assoc($result);
    if ($row === false || $row['VAL'] != 1) {
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
