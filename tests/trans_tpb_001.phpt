--TEST--
fbird_trans_start() with new array options and fbird_trans_info()
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

$db = fbird_connect($test_base);

// Test 1: Read-only transaction
$options = [
    'access_mode' => FBIRD_READ,
    'isolation' => FBIRD_COMMITTED,
    'lock_resolution' => FBIRD_WAIT,
    'lock_timeout' => 2
];

$trans = fbird_trans_start($db, $options);
if (!$trans) {
    die("Failed to start transaction");
}

$info = fbird_trans_info($trans);
var_dump(is_array($info));
var_dump($info['access_mode']); // Should be READ_ONLY
var_dump($info['lock_timeout']); // Should be 2
var_dump($info['isolation']);

fbird_commit($trans);

// Test 2: Read-Write, No Wait
$options2 = [
    'access_mode' => FBIRD_WRITE,
    'isolation' => FBIRD_CONCURRENCY,
    'lock_resolution' => FBIRD_NOWAIT
];
$trans2 = fbird_trans_start($db, $options2);
$info2 = fbird_trans_info($trans2);
var_dump($info2['access_mode']); // READ_WRITE
var_dump($info2['isolation']); // CONCURRENCY

fbird_rollback($trans2);

fbird_close($db);

?>
--EXPECT--
bool(true)
string(9) "READ_ONLY"
int(2)
string(14) "READ_COMMITTED"
string(10) "READ_WRITE"
string(11) "CONCURRENCY"
