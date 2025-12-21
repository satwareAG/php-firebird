--TEST--
IBatch API: fbird_batch_add() binds parameters into message buffer
--SKIPIF--
<?php
require __DIR__ . '/skipif.inc';
if (!extension_loaded('firebird')) {
    die('skip firebird extension not loaded');
}
if (!function_exists('fbird_batch_create')) {
    die('skip IBatch API (fbird_batch_create) not available in this build');
}
?>
--FILE--
<?php
require __DIR__ . '/firebird.inc';

$db = fbird_connect($test_base, $user, $password);
if (!$db) {
    echo "connect failed\n";
    var_dump(fbird_errmsg());
    exit;
}

fbird_query($db, 'DELETE FROM test1');
fbird_commit($db);

$t = fbird_trans($db);

$q = fbird_prepare($t, 'INSERT INTO test1 (i, c) VALUES (?, ?)');

$batch = fbird_batch_create($q, $t);

var_dump(fbird_batch_add($batch, 1, 'hello'));
var_dump(fbird_batch_add($batch, null, 'world')); // NULL integer
var_dump(fbird_batch_add($batch, 3, null)); // NULL varchar

$res = fbird_batch_execute($batch);
var_dump($res);

fbird_commit($t);

$r = fbird_query($db, 'SELECT COUNT(*) FROM test1');
$row = fbird_fetch_row($r);
var_dump($row[0]);

fbird_close($db);
?>
--EXPECTF--
bool(true)
bool(true)
bool(true)
array(2) {
  ["total_processed"]=>
  int(3)
  ["error_count"]=>
  int(0)
}
int(3)
