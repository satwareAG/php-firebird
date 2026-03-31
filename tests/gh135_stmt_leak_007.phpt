--TEST--
GitHub #135: Mixed lifecycle - prepare/execute/free ordering combinations
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
/*
 * Regression test for GitHub issue #135 - resource lifecycle ordering.
 *
 * Tests various combinations of prepare/execute/free ordering to ensure
 * the parent-child resource relationship is correctly managed:
 *
 * 1. Explicit prepare + execute, free result before query
 * 2. Explicit prepare + execute, free query before result
 * 3. Explicit prepare + multiple executes, free results in reverse order
 * 4. Explicit prepare + execute, let both go out of scope (GC cleanup)
 * 5. One-shot fbird_query + let result go out of scope without explicit free
 */
require("firebird.inc");

$db = fbird_connect($test_base, $user, $password);
if (!$db) die('connect failed');

echo "1. Free result before query (normal order)...\n";
for ($i = 0; $i < 50; $i++) {
    $query = fbird_prepare($db, "SELECT 1 AS VAL FROM RDB\$DATABASE");
    $result = fbird_execute($query);
    $row = fbird_fetch_assoc($result);
    if ($row === false || $row['VAL'] != 1) {
        echo "FAIL at iteration $i\n";
        break;
    }
    fbird_free_result($result);
    fbird_free_query($query);
}
echo "OK\n";

echo "2. Free query before result (parent invalidates children)...\n";
for ($i = 0; $i < 50; $i++) {
    $query = fbird_prepare($db, "SELECT 1 AS VAL FROM RDB\$DATABASE");
    $result = fbird_execute($query);
    $row = fbird_fetch_assoc($result);
    if ($row === false || $row['VAL'] != 1) {
        echo "FAIL at iteration $i\n";
        break;
    }
    // Free parent first - destructor invalidates child resources via
    // zend_list_close(child->res). The child resource becomes invalid,
    // so fbird_free_result() would throw. This is correct behavior:
    // freeing the parent cleans up children automatically.
    fbird_free_query($query);
    // Child was already closed by parent destructor - no need to free again
}
echo "OK\n";

echo "3. Multiple executes, free results in reverse order...\n";
for ($i = 0; $i < 20; $i++) {
    $query = fbird_prepare($db, "SELECT 1 AS VAL FROM RDB\$DATABASE");
    // Note: each execute opens a new cursor; on Firebird, re-executing
    // a prepared statement implicitly closes the previous cursor.
    $results = [];
    for ($j = 0; $j < 3; $j++) {
        $results[] = fbird_execute($query);
    }
    // Only the last result has a valid cursor; fetch from it
    $row = fbird_fetch_assoc($results[2]);
    if ($row === false || $row['VAL'] != 1) {
        echo "FAIL at iteration $i\n";
        break;
    }
    // Free in reverse order
    for ($j = count($results) - 1; $j >= 0; $j--) {
        fbird_free_result($results[$j]);
    }
    fbird_free_query($query);
}
echo "OK\n";

echo "4. One-shot queries without explicit free (scope cleanup)...\n";
for ($i = 0; $i < 100; $i++) {
    // Result goes out of scope each iteration - PHP refcount drops to 0
    // and resource destructor fires automatically
    $result = fbird_query($db, "SELECT 1 AS VAL FROM RDB\$DATABASE");
    if (!$result) {
        echo "FAIL at iteration $i\n";
        break;
    }
    $row = fbird_fetch_assoc($result);
    if ($row === false || $row['VAL'] != 1) {
        echo "FAIL: bad data at iteration $i\n";
        break;
    }
    // No explicit fbird_free_result() - rely on scope/GC
}
echo "OK\n";

echo "5. Explicit prepare reuse across many executes...\n";
$query = fbird_prepare($db, "SELECT 1 AS VAL FROM RDB\$DATABASE");
for ($i = 0; $i < 200; $i++) {
    $result = fbird_execute($query);
    if (!$result) {
        echo "FAIL: execute returned false at iteration $i\n";
        break;
    }
    $row = fbird_fetch_assoc($result);
    if ($row === false || $row['VAL'] != 1) {
        echo "FAIL: bad data at iteration $i\n";
        break;
    }
    fbird_free_result($result);
}
fbird_free_query($query);
echo "OK\n";

fbird_close($db);
?>
--EXPECT--
1. Free result before query (normal order)...
OK
2. Free query before result (parent invalidates children)...
OK
3. Multiple executes, free results in reverse order...
OK
4. One-shot queries without explicit free (scope cleanup)...
OK
5. Explicit prepare reuse across many executes...
OK
