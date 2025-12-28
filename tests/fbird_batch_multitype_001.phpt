--TEST--
IBatch API: Comprehensive multi-type batch insert with NULL handling
--SKIPIF--
<?php
require __DIR__ . '/skipif.inc';
if (!extension_loaded('firebird')) {
    die('skip firebird extension not loaded');
}
if (!function_exists('fbird_batch_create')) {
    die('skip IBatch API (fbird_batch_create) not available in this build');
}
if (get_fb_version() < 4.0) {
    die('skip IBatch API requires Firebird 4.0+');
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

echo "=== Phase 1: Create test table ===\n";

// Create comprehensive test table with all column types
$ddl = "RECREATE TABLE BATCH_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    INT_COL INTEGER,
    BIGINT_COL BIGINT,
    SMALLINT_COL SMALLINT,
    FLOAT_COL FLOAT,
    DOUBLE_COL DOUBLE PRECISION,
    NUMERIC_COL NUMERIC(18, 4),
    DECIMAL_COL DECIMAL(10, 2),
    CHAR_COL CHAR(20),
    VARCHAR_COL VARCHAR(100),
    DATE_COL DATE,
    TIME_COL TIME,
    TIMESTAMP_COL TIMESTAMP,
    BOOLEAN_COL BOOLEAN
)";
// Note: BLOBs tested separately in fbird_batch_blob_001.phpt

$r = fbird_query($db, $ddl);
if (!$r) {
    echo "Table creation failed: " . fbird_errmsg() . "\n";
    exit;
}
fbird_commit($db);
echo "Table created successfully\n";

echo "\n=== Phase 2: Batch insert with all types ===\n";

$t = fbird_trans($db);

$sql = "INSERT INTO BATCH_TEST (
    ID, INT_COL, BIGINT_COL, SMALLINT_COL,
    FLOAT_COL, DOUBLE_COL, NUMERIC_COL, DECIMAL_COL,
    CHAR_COL, VARCHAR_COL, DATE_COL, TIME_COL, TIMESTAMP_COL, BOOLEAN_COL
) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

$q = fbird_prepare($t, $sql);
if (!$q) {
    echo "Prepare failed: " . fbird_errmsg() . "\n";
    exit;
}

$batch = fbird_batch_create($q, $t);
if (!$batch) {
    echo "Batch create failed: " . fbird_errmsg() . "\n";
    exit;
}

// Row 1: Full values for all types
$r1 = fbird_batch_add($batch,
    1,                                    // ID
    42,                                   // INT_COL
    9223372036854775807,                  // BIGINT_COL (max int64)
    32767,                                // SMALLINT_COL (max int16)
    3.14159,                              // FLOAT_COL
    2.718281828459045,                    // DOUBLE_COL
    '12345678901234.5678',                // NUMERIC_COL (18,4)
    '12345678.99',                        // DECIMAL_COL (10,2)
    'CHAR_VALUE',                         // CHAR_COL (will be padded)
    'Variable length string',             // VARCHAR_COL
    '2025-12-21',                         // DATE_COL
    '14:30:45',                           // TIME_COL
    '2025-12-21 14:30:45',                // TIMESTAMP_COL
    true                                  // BOOLEAN_COL
);
echo "Row 1 (full values): " . ($r1 ? "added" : "FAILED") . "\n";

// Row 2: NULL handling test - all nullable columns NULL
$r2 = fbird_batch_add($batch,
    2,                                    // ID (NOT NULL)
    null,                                 // INT_COL
    null,                                 // BIGINT_COL
    null,                                 // SMALLINT_COL
    null,                                 // FLOAT_COL
    null,                                 // DOUBLE_COL
    null,                                 // NUMERIC_COL
    null,                                 // DECIMAL_COL
    null,                                 // CHAR_COL
    null,                                 // VARCHAR_COL
    null,                                 // DATE_COL
    null,                                 // TIME_COL
    null,                                 // TIMESTAMP_COL
    null                                  // BOOLEAN_COL
);
echo "Row 2 (all nullable NULLs): " . ($r2 ? "added" : "FAILED") . "\n";

// Row 3: Mixed NULL and values
$r3 = fbird_batch_add($batch,
    3,                                    // ID
    -2147483648,                          // INT_COL (min int32)
    -9223372036854775807,                 // BIGINT_COL (near min int64)
    -32768,                               // SMALLINT_COL (min int16)
    null,                                 // FLOAT_COL (NULL)
    null,                                 // DOUBLE_COL (NULL)
    '-12345678901234.5678',               // NUMERIC_COL (negative)
    '-12345678.99',                       // DECIMAL_COL (negative)
    null,                                 // CHAR_COL (NULL)
    'Mixed NULL test',                    // VARCHAR_COL
    null,                                 // DATE_COL (NULL)
    null,                                 // TIME_COL (NULL)
    '1999-12-31 23:59:59',                // TIMESTAMP_COL (Y2K eve)
    false                                 // BOOLEAN_COL
);
echo "Row 3 (mixed NULL/values): " . ($r3 ? "added" : "FAILED") . "\n";

// Row 4: Edge cases for strings
$r4 = fbird_batch_add($batch,
    4,                                    // ID
    0,                                    // INT_COL (zero)
    0,                                    // BIGINT_COL (zero)
    0,                                    // SMALLINT_COL (zero)
    0.0,                                  // FLOAT_COL (zero)
    0.0,                                  // DOUBLE_COL (zero)
    '0.0000',                             // NUMERIC_COL (zero)
    '0.00',                               // DECIMAL_COL (zero)
    '',                                   // CHAR_COL (empty, will be spaces)
    '',                                   // VARCHAR_COL (empty string)
    '1970-01-01',                         // DATE_COL (Unix epoch)
    '00:00:00',                           // TIME_COL (midnight)
    '1970-01-01 00:00:00',                // TIMESTAMP_COL (Unix epoch)
    false                                 // BOOLEAN_COL
);
echo "Row 4 (edge cases, zeros): " . ($r4 ? "added" : "FAILED") . "\n";

// Execute batch
echo "\n=== Phase 3: Execute batch ===\n";
$res = fbird_batch_execute($batch);
echo "total_processed: " . $res['total_processed'] . "\n";
echo "success_count: " . $res['success_count'] . "\n";
echo "error_count: " . $res['error_count'] . "\n";

fbird_commit($t);

echo "\n=== Phase 4: Verify inserted data ===\n";

// Count rows
$r = fbird_query($db, 'SELECT COUNT(*) FROM BATCH_TEST');
$row = fbird_fetch_row($r);
echo "Total rows inserted: " . $row[0] . "\n";

// Verify row 1 - full values
$r = fbird_query($db, 'SELECT * FROM BATCH_TEST WHERE ID = 1');
$row = fbird_fetch_assoc($r);
echo "\nRow 1 verification:\n";
echo "  INT_COL: " . (is_null($row['INT_COL']) ? "NULL" : $row['INT_COL']) . "\n";
echo "  BIGINT_COL type: " . gettype($row['BIGINT_COL']) . "\n";
echo "  SMALLINT_COL: " . $row['SMALLINT_COL'] . "\n";
echo "  BOOLEAN_COL: " . ($row['BOOLEAN_COL'] === true ? "true" : ($row['BOOLEAN_COL'] === false ? "false" : "NULL")) . "\n";
echo "  VARCHAR_COL: " . $row['VARCHAR_COL'] . "\n";
echo "  CHAR_COL trimmed: '" . trim($row['CHAR_COL']) . "'\n";

// Verify row 2 - all NULLs
$r = fbird_query($db, 'SELECT * FROM BATCH_TEST WHERE ID = 2');
$row = fbird_fetch_assoc($r);
echo "\nRow 2 NULL verification:\n";
$null_count = 0;
foreach (['INT_COL', 'BIGINT_COL', 'SMALLINT_COL', 'FLOAT_COL', 'DOUBLE_COL',
          'NUMERIC_COL', 'DECIMAL_COL', 'CHAR_COL', 'VARCHAR_COL',
          'DATE_COL', 'TIME_COL', 'TIMESTAMP_COL', 'BOOLEAN_COL'] as $col) {
    if (is_null($row[$col])) {
        $null_count++;
    }
}
echo "  NULL columns count: " . $null_count . "/13\n";

// Verify row 4 - zeros and empty strings
$r = fbird_query($db, 'SELECT * FROM BATCH_TEST WHERE ID = 4');
$row = fbird_fetch_assoc($r);
echo "\nRow 4 zero/empty verification:\n";
echo "  INT_COL is zero: " . ($row['INT_COL'] === 0 || $row['INT_COL'] === '0' ? "true" : "false") . "\n";
echo "  VARCHAR_COL is empty string: " . ($row['VARCHAR_COL'] === '' ? "true" : "false") . "\n";
echo "  DATE_COL: " . $row['DATE_COL'] . "\n";

echo "\n=== Phase 5: Error scenario - duplicate primary key ===\n";

$t = fbird_trans($db);
$q = fbird_prepare($t, $sql);
$batch = fbird_batch_create($q, $t);

// Try to insert duplicate ID
fbird_batch_add($batch, 100, 1, 1, 1, 1.0, 1.0, '1.0000', '1.00',
    'TEST', 'TEST', '2025-01-01', '12:00:00', '2025-01-01 12:00:00', true);
fbird_batch_add($batch, 100, 2, 2, 2, 2.0, 2.0, '2.0000', '2.00',  // Duplicate ID!
    'TEST2', 'TEST2', '2025-01-02', '13:00:00', '2025-01-02 13:00:00', true);
fbird_batch_add($batch, 101, 3, 3, 3, 3.0, 3.0, '3.0000', '3.00',
    'TEST3', 'TEST3', '2025-01-03', '14:00:00', '2025-01-03 14:00:00', true);

$res = fbird_batch_execute($batch);
echo "Duplicate key test:\n";
echo "  total_processed: " . $res['total_processed'] . "\n";
echo "  success_count: " . $res['success_count'] . "\n";
echo "  error_count: " . $res['error_count'] . "\n";

// Note: When IBatch encounters an error, by default it stops processing.
// The duplicate key error causes the transaction to be in error state.
// Committing will fail, so use rollback to clean up.
@fbird_rollback($t);

// Verify row 100 does NOT exist (transaction error caused rollback)
$r = fbird_query($db, 'SELECT COUNT(*) FROM BATCH_TEST WHERE ID = 100');
$row = fbird_fetch_row($r);
echo "  Row 100 exists (after error rollback): " . ($row[0] > 0 ? "true" : "false") . "\n";

echo "\n=== Test completed successfully ===\n";

fbird_close($db);
?>
--EXPECTF--
=== Phase 1: Create test table ===
Table created successfully

=== Phase 2: Batch insert with all types ===
Row 1 (full values): added
Row 2 (all nullable NULLs): added
Row 3 (mixed NULL/values): added
Row 4 (edge cases, zeros): added

=== Phase 3: Execute batch ===
total_processed: 4
success_count: 4
error_count: 0

=== Phase 4: Verify inserted data ===
Total rows inserted: 4

Row 1 verification:
  INT_COL: 42
  BIGINT_COL type: %s
  SMALLINT_COL: 32767
  BOOLEAN_COL: true
  VARCHAR_COL: Variable length string
  CHAR_COL trimmed: 'CHAR_VALUE'

Row 2 NULL verification:
  NULL columns count: 13/13

Row 4 zero/empty verification:
  INT_COL is zero: true
  VARCHAR_COL is empty string: true
  DATE_COL: 1970-01-01

=== Phase 5: Error scenario - duplicate primary key ===
Duplicate key test:
  total_processed: 2
  success_count: 1
  error_count: 1
  Row 100 exists (after error rollback): false

=== Test completed successfully ===
