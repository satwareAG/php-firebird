--TEST--
GitHub #135: fbird_query() SELECT must free parent statement immediately (no server leak)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
/*
 * Regression test for GitHub issue #135.
 *
 * Before the fix, each fbird_query() SELECT call accumulated a server-side
 * prepared statement that was never freed until request shutdown. With 1000+
 * iterations this caused massive server RAM growth (1.5 GB reported).
 *
 * The fix transfers statement ownership from the internal parent query to the
 * result resource and frees the parent immediately. When the user calls
 * fbird_free_result() (or the result goes out of scope), the server-side
 * prepared statement is freed.
 *
 * This test verifies correctness (no crashes, correct results) by running
 * fbird_query() in a tight loop with immediate result consumption + free.
 * Actual memory measurement is impractical in .phpt; the test ensures the
 * ownership transfer doesn't break fetch/free semantics.
 */
require("firebird.inc");

$db = fbird_connect($test_base, $user, $password);
if (!$db) die('connect failed');

$iterations = 200;
$success = 0;

for ($i = 0; $i < $iterations; $i++) {
    $result = fbird_query($db, "SELECT 1 AS VAL FROM RDB\$DATABASE");
    if (!$result) {
        echo "FAIL: fbird_query returned false at iteration $i\n";
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

fbird_close($db);
?>
--EXPECT--
OK: 200 iterations completed without error
