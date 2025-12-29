--TEST--
fbird_batch_execute() detailed error reporting
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once 'skipif.inc';
// IBatch API requires Firebird 4.0+
if (!defined('FBIRD_BATCH_WRITE') && !function_exists('fbird_batch_create')) {
    die('skip IBatch API requires Firebird 4.0+');
}
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

echo "=== Test: Detailed Batch Error Reporting ===\n";

// Connect
$db = fbird_connect($test_base, $user, $password);
if (!$db) {
    die("Failed to connect: " . fbird_errmsg());
}

// Create test table with unique constraint
$sql = "EXECUTE BLOCK AS BEGIN
    IF (EXISTS(SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'BATCH_ERROR_TEST')) THEN
        EXECUTE STATEMENT 'DROP TABLE BATCH_ERROR_TEST';
END";
fbird_query($db, $sql);
fbird_commit($db);

$sql = "CREATE TABLE BATCH_ERROR_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    NAME VARCHAR(50) NOT NULL
)";
$result = fbird_query($db, $sql);
if (!$result) {
    die("Failed to create table: " . fbird_errmsg());
}
fbird_commit($db);
echo "Table created successfully\n";

// Start transaction for batch
$trans = fbird_trans($db);
if (!$trans) {
    die("Failed to start transaction: " . fbird_errmsg());
}

// Prepare INSERT statement
$stmt = fbird_prepare($db, $trans, "INSERT INTO BATCH_ERROR_TEST (ID, NAME) VALUES (?, ?)");
if (!$stmt) {
    die("Failed to prepare statement: " . fbird_errmsg());
}
echo "Statement prepared successfully\n";

// Create batch
$batch = fbird_batch_create($stmt, $trans);
if (!$batch) {
    die("Failed to create batch: " . fbird_errmsg());
}
echo "Batch created successfully\n";

// Add rows - some will fail due to duplicate primary key
$rows = [
    [1, 'First'],      // OK
    [2, 'Second'],     // OK
    [1, 'Duplicate'],  // FAIL - duplicate PK
    [3, 'Third'],      // OK
    [2, 'DupAgain'],   // FAIL - duplicate PK
];

foreach ($rows as $i => $row) {
    $added = fbird_batch_add($batch, $row[0], $row[1]);
    if (!$added) {
        echo "Warning: Failed to add row $i: " . fbird_errmsg() . "\n";
    }
}
echo "Added " . count($rows) . " rows to batch\n";

// Execute batch - expect mixed results
$result = fbird_batch_execute($batch);
var_dump($result);

// Check result structure
if (!is_array($result)) {
    die("FAIL: Result should be an array\n");
}

// Verify expected keys
$expected_keys = ['total_processed', 'success_count', 'error_count'];
foreach ($expected_keys as $key) {
    if (!array_key_exists($key, $result)) {
        echo "WARNING: Missing key '$key' in result\n";
    }
}

// Check counts
echo "\nResult summary:\n";
echo "  Total processed: " . ($result['total_processed'] ?? 'N/A') . "\n";
echo "  Success count: " . ($result['success_count'] ?? 'N/A') . "\n";
echo "  Error count: " . ($result['error_count'] ?? 'N/A') . "\n";

// If errors array exists, display detailed errors
if (isset($result['errors']) && is_array($result['errors'])) {
    echo "\nDetailed errors:\n";
    foreach ($result['errors'] as $error) {
        echo "  Position " . $error['position'] . ": ";
        echo "[" . ($error['sqlstate'] ?? '?????') . "] ";
        echo ($error['message'] ?? 'Unknown error') . "\n";
    }
}

// Verify we have some successful inserts
fbird_commit($trans);

$query = fbird_query($db, "SELECT COUNT(*) FROM BATCH_ERROR_TEST");
$row = fbird_fetch_row($query);
$count = $row[0];
fbird_free_result($query);

echo "\nRows inserted: $count\n";

// Expected: 2 rows (ID=1, ID=2) - Firebird IBatch stops processing after first error
// Batch order: Row 0 (ID=1, OK) -> Row 1 (ID=2, OK) -> Row 2 (ID=1 dup, FAIL) -> stops
// Rows 3 and 4 are never processed because IBatch stops at first error
if ($count == 2) {
    echo "PASS: Correct number of rows inserted (IBatch stopped at first error)\n";
} else if ($count > 0) {
    echo "PARTIAL: Some rows inserted ($count), expected 2\n";
} else {
    echo "FAIL: No rows inserted\n";
}

// Cleanup
fbird_query($db, "DROP TABLE BATCH_ERROR_TEST");
fbird_commit($db);
fbird_close($db);

echo "\n=== Test Complete ===\n";
?>
--EXPECTF--
=== Test: Detailed Batch Error Reporting ===
Table created successfully
Statement prepared successfully
Batch created successfully
Added 5 rows to batch
array(%d) {
  ["total_processed"]=>
  int(3)
  ["success_count"]=>
  int(2)
  ["error_count"]=>
  int(1)
%A}

Result summary:
  Total processed: 3
  Success count: 2
  Error count: 1
%A
Rows inserted: 2
PASS: Correct number of rows inserted (IBatch stopped at first error)
%A
=== Test Complete ===
