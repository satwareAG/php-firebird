--TEST--
fbird_query: FB 4.0+ server-side statement cleanup via IStatement::free() and IResultSet::close()
--EXTENSIONS--
firebird
--SKIPIF--
<?php
include("skipif.inc");
require("firebird.inc");
$conn = fbird_connect($test_base, $user, $password);
if (!$conn) die("skip: cannot connect to Firebird");
$ver = fbird_server_info($conn, IBASE_SVC_SERVER_VERSION);
if (preg_match('/LI-V(\d+)\./', $ver ?? '', $m) && (int)$m[1] < 4) {
    fbird_close($conn);
    die("skip: Firebird server version " . $m[1] . ".x < 4.0");
}
fbird_close($conn);
?>
--FILE--
<?php
require("firebird.inc");

$conn = fbird_connect($test_base, $user, $password);
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
