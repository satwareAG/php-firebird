--TEST--
Firebird\Batch userland wrapper: fromQuery, add, execute, getRowCount, isExecuted, cancel
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird extension not loaded');
require_once __DIR__ . '/config.inc';
if (!@fbird_connect($test_base, $user, $password)) die('skip cannot connect to Firebird');
if (!function_exists('fbird_batch_create')) die('skip IBatch API requires Firebird 4.0+');
?>
--FILE--
<?php
require_once __DIR__ . '/config.inc';
require_once __DIR__ . '/../vendor/autoload.php';

use Firebird\Batch;
use Firebird\BatchResult;

$db = fbird_connect($test_base, $user, $password);

// Create test table
@fbird_query($db, 'RECREATE TABLE OO_BATCH_TEST (ID INTEGER NOT NULL PRIMARY KEY, NAME VARCHAR(50))');
fbird_commit($db);

$stmt = fbird_prepare($db, 'INSERT INTO OO_BATCH_TEST (ID, NAME) VALUES (?, ?)');
var_dump($stmt !== false);

// fromQuery() factory
$batch = Batch::fromQuery($stmt);
var_dump($batch instanceof Batch);

// initial state
var_dump($batch->isExecuted() === false);
var_dump($batch->getRowCount() === 0);
var_dump($batch->getResource() !== null);
var_dump(is_array($batch->getBlobIds()));

// add() returns self (fluent)
$b2 = $batch->add(1, 'Alice');
var_dump($b2 instanceof Batch);
$batch->add(2, 'Bob');
$batch->add(3, 'Carol');
var_dump($batch->getRowCount() === 3);

// execute() returns BatchResult
$result = $batch->execute();
var_dump($result instanceof BatchResult);
var_dump($batch->isExecuted());
var_dump($result->totalRows === 3);
var_dump($result->successCount === 3);
var_dump($result->hasErrors() === false);
var_dump($result->isComplete());

fbird_commit($db);

// cancel() on a fresh batch
$stmt2 = fbird_prepare($db, 'INSERT INTO OO_BATCH_TEST (ID, NAME) VALUES (?, ?)');
$batch2 = Batch::fromQuery($stmt2);
$batch2->add(4, 'Dave');
$batch2->cancel();
var_dump($batch2->getRowCount() === 0);

fbird_close($db);
echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
done
