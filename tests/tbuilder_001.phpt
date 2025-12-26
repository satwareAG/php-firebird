--TEST--
TBuilder: Fluent transaction parameter builder
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");
require_once __DIR__ . '/../src/Firebird/TBuilder.php';

use Firebird\TBuilder;

$db = fbird_connect($test_base);

// Test 1: Basic read-only transaction with TBuilder
echo "Test 1: Read-only transaction\n";
$options = TBuilder::create()
    ->readOnly()
    ->isolationReadCommitted()
    ->wait(5)
    ->build();

var_dump($options);

$trans = fbird_trans_start($db, $options);
if (!$trans) {
    die("Failed to start transaction");
}

$info = fbird_trans_info($trans);
var_dump($info['access_mode']);
fbird_commit($trans);

// Test 2: Snapshot isolation with no wait
echo "\nTest 2: Snapshot with no wait\n";
$options2 = TBuilder::create()
    ->readWrite()
    ->isolationSnapshot()
    ->noWait()
    ->build();

var_dump($options2);

$trans2 = fbird_trans_start($db, $options2);
$info2 = fbird_trans_info($trans2);
var_dump($info2['isolation']);
fbird_rollback($trans2);

// Test 3: buildFlags() for legacy usage
echo "\nTest 3: Legacy buildFlags()\n";
$builder = TBuilder::create()
    ->readOnly()
    ->isolationConsistency()
    ->wait();

$flags = $builder->buildFlags();
var_dump(($flags & FBIRD_READ) === FBIRD_READ);
var_dump(($flags & FBIRD_CONSISTENCY) === FBIRD_CONSISTENCY);
var_dump(($flags & FBIRD_WAIT) === FBIRD_WAIT);

// Test 4: Table reservations
echo "\nTest 4: Table reservations\n";
$options4 = TBuilder::create()
    ->isolationSnapshotTableStability()
    ->reserveProtectedWrite('TEST_TABLE')
    ->build();

var_dump(isset($options4['table_reservations']));
var_dump(isset($options4['table_reservations']['TEST_TABLE']));

// Test 5: Copy and reset
echo "\nTest 5: Copy and reset\n";
$builder5 = TBuilder::create()
    ->readOnly()
    ->isolationReadCommittedReadConsistency();

$copy = $builder5->copy();
$builder5->reset();

$copyOptions = $copy->build();
$resetOptions = $builder5->build();

var_dump(isset($copyOptions['access_mode']));
var_dump(empty($resetOptions));

// Test 6: Lock timeout getter
echo "\nTest 6: Lock timeout getter\n";
$builder6 = TBuilder::create()->wait(10);
var_dump($builder6->getLockTimeout());

$builder6b = TBuilder::create()->wait();
var_dump($builder6b->getLockTimeout());

fbird_close($db);

echo "\nDone.\n";
?>
--EXPECT--
Test 1: Read-only transaction
array(4) {
  ["access_mode"]=>
  int(2)
  ["isolation"]=>
  int(72)
  ["lock_resolution"]=>
  int(128)
  ["lock_timeout"]=>
  int(5)
}
string(9) "READ_ONLY"

Test 2: Snapshot with no wait
array(3) {
  ["access_mode"]=>
  int(1)
  ["isolation"]=>
  int(4)
  ["lock_resolution"]=>
  int(256)
}
string(11) "CONCURRENCY"

Test 3: Legacy buildFlags()
bool(true)
bool(true)
bool(true)

Test 4: Table reservations
bool(true)
bool(true)

Test 5: Copy and reset
bool(true)
bool(true)

Test 6: Lock timeout getter
int(10)
NULL

Done.
