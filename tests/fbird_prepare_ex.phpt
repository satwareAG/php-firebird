--TEST--
fbird_prepare_ex() with fixed signature
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
require_once __DIR__ . '/firebird.inc';
$dsn = $test_base;
$c = @fbird_connect($dsn, $user, $password);
if (!$c) die('skip cannot connect');
fbird_close($c);
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

$dsn = $test_base;
$conn = fbird_connect($dsn, $user, $password);

/* Basic prepare with link + query */
$stmt = fbird_prepare_ex($conn, "SELECT 1 AS val FROM RDB\$DATABASE");
if (!$stmt) {
    die("FAIL: prepare failed: " . fbird_errmsg() . "\n");
}
$result = fbird_execute($stmt);
$row = fbird_fetch_assoc($result);
echo "val: " . $row['VAL'] . "\n";
fbird_free_result($result);
fbird_free_query($stmt);

/* Prepare with explicit transaction */
$trans = fbird_trans_start($conn);
$stmt = fbird_prepare_ex($conn, "SELECT 2 AS val FROM RDB\$DATABASE", $trans);
if (!$stmt) {
    die("FAIL: prepare with trans failed: " . fbird_errmsg() . "\n");
}
$result = fbird_execute($stmt);
$row = fbird_fetch_assoc($result);
echo "val2: " . $row['VAL'] . "\n";
fbird_free_result($result);
fbird_free_query($stmt);
fbird_commit($trans);

fbird_close($conn);
echo "Done\n";
?>
--EXPECT--
val: 1
val2: 2
Done
