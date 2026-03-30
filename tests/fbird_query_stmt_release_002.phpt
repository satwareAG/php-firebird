--TEST--
fbird_query: FB 4.0+ server-side statement cleanup via IStatement::free() and IResultSet::close()
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once 'config.inc';
$conn = fbird_connect(FIREBIRD_TEST_DB, FIREBIRD_TEST_USER, FIREBIRD_TEST_PASS);
if (!$conn) die("skip: cannot connect to Firebird");
$ver = fbird_server_info($conn, IBASE_SVC_SERVER_VERSION);
// Skip on FB 3.0 client: this test verifies FB 4+ behaviour
// FB_API_VER is compile-time; skip at runtime if server < 4
if (preg_match('/^[Ww][Ii][Nn]|^[Uu][Ss][Ee]|LI-V3/', $ver ?? '')) {
    fbird_close($conn);
    die("skip: Firebird server < 4.0 (test targets FB 4+ resource cleanup)");
}
// Check version number: FB4 returns "LI-V4.x.y", FB5 returns "LI-V5.x.y" etc.
if (preg_match('/LI-V(\d+)\./', $ver ?? '', $m) && (int)$m[1] < 4) {
    fbird_close($conn);
    die("skip: Firebird server version " . $m[1] . ".x < 4.0");
}
fbird_close($conn);
?>
--FILE--
<?php
require_once 'config.inc';

$conn = fbird_connect(FIREBIRD_TEST_DB, FIREBIRD_TEST_USER, FIREBIRD_TEST_PASS);
if (!$conn) {
    echo "FAILED: connect\n";
    exit(1);
}

$trans = fbird_trans($conn);
if (!$trans) {
    echo "FAILED: trans\n";
    fbird_close($conn);
    exit(1);
}

// Create test table
@fbird_query($trans, 'DROP TABLE fbird_stmt_release_002');
$res = fbird_query($trans, 'CREATE TABLE fbird_stmt_release_002 (id INTEGER NOT NULL PRIMARY KEY, val VARCHAR(50))');
if ($res === false) {
    echo "FAILED: create table\n";
    fbird_rollback($trans);
    fbird_close($conn);
    exit(1);
}
fbird_commit($trans);

$trans = fbird_trans($conn);

// Execute 500 DML statements - verify they complete without error
// On FB 4+, IStatement::free() ensures server resources are released promptly
$ok = true;
for ($i = 1; $i <= 500; $i++) {
    $res = fbird_query($trans, "INSERT INTO fbird_stmt_release_002 (id, val) VALUES ($i, 'value_$i')");
    if ($res === false) {
        $ok = false;
        echo "FAILED: INSERT $i: " . fbird_errmsg() . "\n";
        break;
    }
}

if ($ok) {
    echo "500 DML statements completed without error\n";
}

fbird_commit($trans);

// Execute 50 SELECT queries - verify IResultSet::close() works on FB 4+
$trans = fbird_trans($conn);
$select_ok = true;
for ($i = 1; $i <= 50; $i++) {
    $res = fbird_query($trans, "SELECT id, val FROM fbird_stmt_release_002 WHERE id = $i");
    if ($res === false) {
        $select_ok = false;
        echo "FAILED: SELECT $i: " . fbird_errmsg() . "\n";
        break;
    }
    $row = fbird_fetch_assoc($res);
    if (!$row || $row['ID'] != $i) {
        $select_ok = false;
        echo "FAILED: unexpected row for id=$i\n";
        break;
    }
    fbird_free_result($res);
}

if ($select_ok) {
    echo "50 SELECT statements completed without error\n";
}

// Cleanup
fbird_commit($trans);
$trans = fbird_trans($conn);
fbird_query($trans, 'DROP TABLE fbird_stmt_release_002');
fbird_commit($trans);
fbird_close($conn);

echo "DONE\n";
?>
--EXPECT--
500 DML statements completed without error
50 SELECT statements completed without error
DONE
