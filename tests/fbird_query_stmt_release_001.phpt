--TEST--
fbird_query: server-side prepared statements are freed after DML execution (issue #135)
--SKIPIF--
<?php
require __DIR__ . '/skipif.inc';
?>
--FILE--
<?php
require __DIR__ . '/firebird.inc';

$db = fbird_connect($test_base, $user, $password);
if (!$db) {
    die("FAIL: cannot connect: " . fbird_errmsg());
}

// Create a temporary test table
fbird_query($db, 'CREATE TABLE stmt_release_test (id INTEGER NOT NULL PRIMARY KEY, val VARCHAR(32))');
fbird_commit($db);

// Execute many DML queries via fbird_query() without storing the return value.
// Before fix: each call left a prepared statement handle alive on the Firebird server
// until request shutdown, accumulating server RAM over thousands of calls.
// After fix: StatementWrapper::free() calls IStatement::free() (DSQL_drop equivalent)
// immediately after execution, so no server-side statements accumulate (issue #135).
$iterations = 500;
for ($i = 0; $i < $iterations; $i++) {
    $result = fbird_query($db, "INSERT INTO stmt_release_test (id, val) VALUES ($i, 'row_$i')");
    if ($result === false) {
        @fbird_query($db, 'DROP TABLE stmt_release_test');
        @fbird_commit($db);
        die("FAIL: INSERT $i failed: " . fbird_errmsg());
    }
}
fbird_commit($db);

// Verify all rows were inserted correctly
$result = fbird_query($db, 'SELECT COUNT(*) AS CNT FROM stmt_release_test');
$row = fbird_fetch_assoc($result);
fbird_free_result($result);

if ((int)$row['CNT'] !== $iterations) {
    @fbird_query($db, 'DROP TABLE stmt_release_test');
    @fbird_commit($db);
    die("FAIL: expected $iterations rows, got {$row['CNT']}");
}

// Also test UPDATE statements (another common DML path)
for ($i = 0; $i < 100; $i++) {
    $result = fbird_query($db, "UPDATE stmt_release_test SET val = 'updated_$i' WHERE id = $i");
    if ($result === false) {
        @fbird_query($db, 'DROP TABLE stmt_release_test');
        @fbird_commit($db);
        die("FAIL: UPDATE $i failed: " . fbird_errmsg());
    }
}
fbird_commit($db);

// Verify connection still alive and usable after 600+ DML statements
$result = fbird_query($db, "SELECT COUNT(*) AS CNT FROM stmt_release_test WHERE val LIKE 'updated_%'");
$row = fbird_fetch_assoc($result);
fbird_free_result($result);

if ((int)$row['CNT'] !== 100) {
    @fbird_query($db, 'DROP TABLE stmt_release_test');
    @fbird_commit($db);
    die("FAIL: expected 100 updated rows, got {$row['CNT']}");
}

// Cleanup: close connection first to release all server-side locks,
// then reconnect for DDL cleanup (avoids FB3 "object is in use" on DROP TABLE)
fbird_close($db);
$db2 = fbird_connect($test_base, $user, $password);
fbird_query($db2, 'DROP TABLE stmt_release_test');
fbird_commit($db2);
fbird_close($db2);

echo "ok\n";
?>
--EXPECT--
ok
