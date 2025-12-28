--TEST--
Firebird: IBatch OO Wrapper (Batch, BatchResult, BatchError classes)
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require __DIR__ . '/skipif.inc';
if (!function_exists('fbird_batch_create')) {
    die('skip: IBatch API requires Firebird 4.0+');
}
if (get_fb_version() < 4.0) {
    die('skip IBatch API requires Firebird 4.0+');
}
?>
--FILE--
<?php
declare(strict_types=1);

require __DIR__ . '/firebird.inc';
require_once dirname(__DIR__) . '/src/Firebird/BlobId.php';
require_once dirname(__DIR__) . '/src/Firebird/BatchError.php';
require_once dirname(__DIR__) . '/src/Firebird/BatchResult.php';
require_once dirname(__DIR__) . '/src/Firebird/Batch.php';

use Firebird\Batch;
use Firebird\BatchResult;
use Firebird\BatchError;

// Connect
$db = fbird_connect($test_base, $user, $password);
if (!$db) {
    die('Could not connect to database');
}

echo "=== Test 1: BatchError value object ===\n";

// Test BatchError factory
$error = BatchError::fromArray([
    'position' => 2,
    'sqlstate' => '23000',
    'message' => 'Duplicate key violation'
]);

var_dump($error->position);
var_dump($error->sqlstate);
var_dump($error->message);
var_dump($error->isConstraintViolation());
var_dump($error->isSyntaxError());
var_dump($error->getErrorClass());
echo "String: " . $error . "\n";

echo "\n=== Test 2: BatchResult value object ===\n";

// Test BatchResult factory
$result = BatchResult::fromArray([
    'total_processed' => 10,
    'success_count' => 8,
    'error_count' => 2,
    'errors' => [
        ['position' => 2, 'sqlstate' => '23000', 'message' => 'Duplicate key'],
        ['position' => 7, 'sqlstate' => '22012', 'message' => 'Division by zero']
    ]
]);

var_dump($result->totalRows);
var_dump($result->successCount);
var_dump($result->errorCount);
var_dump($result->hasErrors());
var_dump($result->isComplete());
var_dump(round($result->getSuccessRate(), 2));
var_dump(count($result));  // Countable
var_dump($result->getFirstError()?->position);

echo "Summary: " . $result->getSummary() . "\n";

// Test IteratorAggregate
echo "Iterating errors:\n";
foreach ($result as $err) {
    echo "  - Position {$err->position}: {$err->sqlstate}\n";
}

echo "\n=== Test 3: BatchResult without errors ===\n";

$successResult = BatchResult::fromArray([
    'total_processed' => 5,
    'success_count' => 5,
    'error_count' => 0
]);

var_dump($successResult->hasErrors());
var_dump($successResult->isComplete());
var_dump($successResult->getFirstError() === null);

echo "\n=== Test 4: Batch OO wrapper ===\n";

// Create test table
$createSql = "RECREATE TABLE BATCH_OO_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    NAME VARCHAR(50)
)";
@fbird_query($db, $createSql);
fbird_commit($db);

// Prepare statement
$stmt = fbird_prepare($db, "INSERT INTO BATCH_OO_TEST (ID, NAME) VALUES (?, ?)");
if (!$stmt) {
    die('Prepare failed: ' . fbird_errmsg());
}

// Create Batch from query resource
$batch = Batch::fromQuery($stmt);
var_dump($batch instanceof Batch);
var_dump($batch->getRowCount());

// Add rows using fluent interface
$batch->add(1, 'Alice')
      ->add(2, 'Bob')
      ->add(3, 'Charlie');

var_dump($batch->getRowCount());

// Execute batch
$batchResult = $batch->execute();
var_dump($batchResult instanceof BatchResult);
var_dump($batchResult->successCount);
var_dump($batchResult->hasErrors());

fbird_commit($db);

echo "\n=== Test 5: Batch with errors ===\n";

// Create new batch with duplicate key
$stmt2 = fbird_prepare($db, "INSERT INTO BATCH_OO_TEST (ID, NAME) VALUES (?, ?)");
$batch2 = Batch::fromQuery($stmt2);

$batch2->add(10, 'Item10')  // OK
       ->add(10, 'Dup10')   // Duplicate - will fail
       ->add(11, 'Item11'); // May not be processed (IBatch stops on first error)

$result2 = $batch2->execute();
var_dump($result2->hasErrors());
var_dump($result2->errorCount >= 1);

fbird_commit($db);

// Verify data
$rs = fbird_query($db, "SELECT COUNT(*) AS CNT FROM BATCH_OO_TEST");
$row = fbird_fetch_assoc($rs);
echo "Total rows in table: " . $row['CNT'] . "\n";
fbird_free_result($rs);

// Free resources to release table locks
unset($batch, $batch2);
fbird_free_query($stmt);
fbird_free_query($stmt2);

fbird_commit($db); // Commit read transaction

// Cleanup
fbird_close($db);

// Reconnect to drop table (ensures all locks are released)
$db = fbird_connect($test_base, $user, $password);
fbird_query($db, "DROP TABLE BATCH_OO_TEST");
fbird_commit($db);
fbird_close($db);

echo "\nDone!\n";
?>
--EXPECTF--
=== Test 1: BatchError value object ===
int(2)
string(5) "23000"
string(23) "Duplicate key violation"
bool(true)
bool(false)
string(2) "23"
String: Row 2: [23000] Duplicate key violation

=== Test 2: BatchResult value object ===
int(10)
int(8)
int(2)
bool(true)
bool(false)
float(0.8)
int(2)
int(2)
Summary: Batch complete: 8/10 succeeded (80.0%), 2 errors
Iterating errors:
  - Position 2: 23000
  - Position 7: 22012

=== Test 3: BatchResult without errors ===
bool(false)
bool(true)
bool(true)

=== Test 4: Batch OO wrapper ===
bool(true)
int(0)
int(3)
bool(true)
int(3)
bool(false)

=== Test 5: Batch with errors ===
bool(true)
bool(true)
Total rows in table: %d
%A
Done!
