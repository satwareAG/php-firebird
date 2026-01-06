--TEST--
Coverage: NUMERIC/DECIMAL scale handling (fbird_query_bind.c)
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once __DIR__ . '/../skipif.inc';
?>
--FILE--
<?php
/**
 * Coverage test for NUMERIC/DECIMAL scale handling in fbird_query_bind.c
 *
 * Tests scale conversion paths in _php_fbird_safe_copy_sqlvar_data():
 * - SQL_SHORT with scale (NUMERIC(4,x))
 * - SQL_LONG with scale (NUMERIC(9,x))
 * - SQL_INT64 with scale (NUMERIC(18,x))
 * - Scale values 0-18
 * - Positive and negative scaled values
 *
 * Target: Cover scale handling paths in fbird_query_bind.c lines 150-300
 */
require_once __DIR__ . '/../firebird.inc';

echo "=== Test: NUMERIC/DECIMAL Scale Handling ===\n";

$db = fbird_connect($test_base, $user, $password);
if (!$db) {
    die("Connection failed: " . fbird_errmsg() . "\n");
}

// Clean up any existing test table
@fbird_query($db, 'DROP TABLE NUMERIC_SCALE_TEST');
@fbird_commit($db);

// Create test table with various NUMERIC/DECIMAL precisions and scales
$create = "
CREATE TABLE NUMERIC_SCALE_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    
    -- SMALLINT-backed NUMERIC (precision 1-4)
    NUM_4_0 NUMERIC(4,0),
    NUM_4_2 NUMERIC(4,2),
    NUM_4_4 NUMERIC(4,4),
    
    -- INTEGER-backed NUMERIC (precision 5-9)
    NUM_9_0 NUMERIC(9,0),
    NUM_9_3 NUMERIC(9,3),
    NUM_9_6 NUMERIC(9,6),
    NUM_9_9 NUMERIC(9,9),
    
    -- BIGINT-backed NUMERIC (precision 10-18)
    NUM_18_0 NUMERIC(18,0),
    NUM_18_4 NUMERIC(18,4),
    NUM_18_8 NUMERIC(18,8),
    NUM_18_12 NUMERIC(18,12),
    NUM_18_18 NUMERIC(18,18),
    
    -- DECIMAL types (same internal representation)
    DEC_10_2 DECIMAL(10,2),
    DEC_15_5 DECIMAL(15,5),
    DEC_18_10 DECIMAL(18,10)
)";
fbird_query($db, $create);
fbird_commit($db);
echo "Test table created\n";

// Test 1: SMALLINT-backed NUMERIC (scale 0-4)
echo "\n--- Test 1: SMALLINT-backed NUMERIC ---\n";
$stmt = fbird_prepare($db, "INSERT INTO NUMERIC_SCALE_TEST (ID, NUM_4_0, NUM_4_2, NUM_4_4) VALUES (?, ?, ?, ?)");

// Integer values
$result = fbird_execute($stmt, 1, 1234, 12.34, 0.1234);
var_dump($result !== false);

// Edge values
$result = fbird_execute($stmt, 2, 9999, 99.99, 0.9999);
var_dump($result !== false);

// Negative values
$result = fbird_execute($stmt, 3, -5678, -56.78, -0.5678);
var_dump($result !== false);

fbird_commit($db);

// Verify
$r = fbird_query($db, "SELECT ID, NUM_4_0, NUM_4_2, NUM_4_4 FROM NUMERIC_SCALE_TEST WHERE ID <= 3 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    echo "ID={$row['ID']}: NUM_4_0={$row['NUM_4_0']}, NUM_4_2={$row['NUM_4_2']}, NUM_4_4={$row['NUM_4_4']}\n";
}
fbird_free_result($r);

// Test 2: INTEGER-backed NUMERIC (scale 0-9)
echo "\n--- Test 2: INTEGER-backed NUMERIC ---\n";
$stmt = fbird_prepare($db, "INSERT INTO NUMERIC_SCALE_TEST (ID, NUM_9_0, NUM_9_3, NUM_9_6, NUM_9_9) VALUES (?, ?, ?, ?, ?)");

$result = fbird_execute($stmt, 10, 123456789, 123456.789, 123.456789, 0.123456789);
var_dump($result !== false);

$result = fbird_execute($stmt, 11, 999999999, 999999.999, 999.999999, 0.999999999);
var_dump($result !== false);

$result = fbird_execute($stmt, 12, -987654321, -987654.321, -987.654321, -0.987654321);
var_dump($result !== false);

fbird_commit($db);

// Verify
$r = fbird_query($db, "SELECT ID, NUM_9_0, NUM_9_3, NUM_9_6, NUM_9_9 FROM NUMERIC_SCALE_TEST WHERE ID >= 10 AND ID < 20 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    echo "ID={$row['ID']}: NUM_9_0={$row['NUM_9_0']}, NUM_9_3={$row['NUM_9_3']}, NUM_9_6={$row['NUM_9_6']}, NUM_9_9={$row['NUM_9_9']}\n";
}
fbird_free_result($r);

// Test 3: BIGINT-backed NUMERIC (scale 0-18)
echo "\n--- Test 3: BIGINT-backed NUMERIC ---\n";
$stmt = fbird_prepare($db, "INSERT INTO NUMERIC_SCALE_TEST (ID, NUM_18_0, NUM_18_4, NUM_18_8, NUM_18_12, NUM_18_18) VALUES (?, ?, ?, ?, ?, ?)");

// Large precision values
$result = fbird_execute($stmt, 20, 
    123456789012345678,  // 18 digits no scale
    12345678901234.5678, // 14.4 digits
    1234567890.12345678, // 10.8 digits
    123456.123456789012, // 6.12 digits
    0.123456789012345678 // 0.18 digits
);
var_dump($result !== false);

// Values with many decimal places
$result = fbird_execute($stmt, 21, 1, 0.0001, 0.00000001, 0.000000000001, 0.000000000000000001);
var_dump($result !== false);

// Negative large values
$result = fbird_execute($stmt, 22, -999999999999999999, -99999999999999.9999, -9999999999.99999999, -999999.999999999999, -0.999999999999999999);
var_dump($result !== false);

fbird_commit($db);

// Verify (use CAST to string for precision)
$r = fbird_query($db, "SELECT ID, NUM_18_0, NUM_18_4, NUM_18_8, NUM_18_12, NUM_18_18 FROM NUMERIC_SCALE_TEST WHERE ID >= 20 AND ID < 30 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    echo "ID={$row['ID']}: NUM_18_0={$row['NUM_18_0']}, NUM_18_4={$row['NUM_18_4']}\n";
}
fbird_free_result($r);

// Test 4: DECIMAL types
echo "\n--- Test 4: DECIMAL types ---\n";
$stmt = fbird_prepare($db, "INSERT INTO NUMERIC_SCALE_TEST (ID, DEC_10_2, DEC_15_5, DEC_18_10) VALUES (?, ?, ?, ?)");

$result = fbird_execute($stmt, 30, 12345678.90, 1234567890.12345, 12345678.1234567890);
var_dump($result !== false);

$result = fbird_execute($stmt, 31, -99999999.99, -9999999999.99999, -99999999.9999999999);
var_dump($result !== false);

// Zero values
$result = fbird_execute($stmt, 32, 0.00, 0.00000, 0.0000000000);
var_dump($result !== false);

fbird_commit($db);

// Verify
$r = fbird_query($db, "SELECT ID, DEC_10_2, DEC_15_5, DEC_18_10 FROM NUMERIC_SCALE_TEST WHERE ID >= 30 AND ID < 40 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    echo "ID={$row['ID']}: DEC_10_2={$row['DEC_10_2']}, DEC_15_5={$row['DEC_15_5']}, DEC_18_10={$row['DEC_18_10']}\n";
}
fbird_free_result($r);

// Test 5: Type coercion to scaled numeric
echo "\n--- Test 5: Type coercion to scaled numeric ---\n";
$stmt = fbird_prepare($db, "INSERT INTO NUMERIC_SCALE_TEST (ID, NUM_9_3, DEC_10_2) VALUES (?, ?, ?)");

// Integer to scaled
$result = fbird_execute($stmt, 40, 12345, 9999);
var_dump($result !== false);

// String to scaled
$result = fbird_execute($stmt, 41, "123.456", "1234.56");
var_dump($result !== false);

// Float to scaled
$result = fbird_execute($stmt, 42, 789.123456789, 5678.901234);
var_dump($result !== false);

fbird_commit($db);

// Verify
$r = fbird_query($db, "SELECT ID, NUM_9_3, DEC_10_2 FROM NUMERIC_SCALE_TEST WHERE ID >= 40 AND ID < 50 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    echo "ID={$row['ID']}: NUM_9_3={$row['NUM_9_3']}, DEC_10_2={$row['DEC_10_2']}\n";
}
fbird_free_result($r);

// Test 6: NULL values in scaled columns
echo "\n--- Test 6: NULL values in scaled columns ---\n";
$stmt = fbird_prepare($db, "INSERT INTO NUMERIC_SCALE_TEST (ID, NUM_4_2, NUM_9_3, NUM_18_4, DEC_10_2) VALUES (?, ?, ?, ?, ?)");

$result = fbird_execute($stmt, 50, null, null, null, null);
var_dump($result !== false);

$result = fbird_execute($stmt, 51, 12.34, null, 1234.5678, null);
var_dump($result !== false);

fbird_commit($db);

// Verify NULLs
$r = fbird_query($db, "SELECT ID, NUM_4_2, NUM_9_3, NUM_18_4, DEC_10_2 FROM NUMERIC_SCALE_TEST WHERE ID >= 50 AND ID < 60 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    $num_4_2 = $row['NUM_4_2'] ?? 'NULL';
    $num_9_3 = $row['NUM_9_3'] ?? 'NULL';
    $num_18_4 = $row['NUM_18_4'] ?? 'NULL';
    $dec_10_2 = $row['DEC_10_2'] ?? 'NULL';
    echo "ID={$row['ID']}: NUM_4_2=$num_4_2, NUM_9_3=$num_9_3, NUM_18_4=$num_18_4, DEC_10_2=$dec_10_2\n";
}
fbird_free_result($r);

// Test 7: Boundary values
echo "\n--- Test 7: Boundary values ---\n";
$stmt = fbird_prepare($db, "INSERT INTO NUMERIC_SCALE_TEST (ID, NUM_4_0, NUM_9_0, NUM_18_0) VALUES (?, ?, ?, ?)");

// Maximum positive values
$result = fbird_execute($stmt, 60, 9999, 999999999, 999999999999999999);
var_dump($result !== false);

// Maximum negative values
$result = fbird_execute($stmt, 61, -9999, -999999999, -999999999999999999);
var_dump($result !== false);

// Minimum positive values
$result = fbird_execute($stmt, 62, 1, 1, 1);
var_dump($result !== false);

fbird_commit($db);

// Verify
$r = fbird_query($db, "SELECT ID, NUM_4_0, NUM_9_0, NUM_18_0 FROM NUMERIC_SCALE_TEST WHERE ID >= 60 AND ID < 70 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    echo "ID={$row['ID']}: NUM_4_0={$row['NUM_4_0']}, NUM_9_0={$row['NUM_9_0']}, NUM_18_0={$row['NUM_18_0']}\n";
}
fbird_free_result($r);

// Final count
$r = fbird_query($db, "SELECT COUNT(*) AS CNT FROM NUMERIC_SCALE_TEST");
$row = fbird_fetch_assoc($r);
echo "\n--- Total rows inserted: " . $row['CNT'] . " ---\n";
fbird_free_result($r);

// Cleanup
echo "\n--- Cleanup ---\n";
@fbird_query($db, 'DROP TABLE NUMERIC_SCALE_TEST');
@fbird_commit($db);
fbird_close($db);

echo "\n=== Test Complete ===\n";
?>
--EXPECTF--
=== Test: NUMERIC/DECIMAL Scale Handling ===
Test table created

--- Test 1: SMALLINT-backed NUMERIC ---
bool(true)
bool(true)
bool(true)
ID=1: NUM_4_0=1234, NUM_4_2=12.34, NUM_4_4=0.1234
ID=2: NUM_4_0=9999, NUM_4_2=99.99, NUM_4_4=0.9999
ID=3: NUM_4_0=-5678, NUM_4_2=-56.78, NUM_4_4=-0.5678

--- Test 2: INTEGER-backed NUMERIC ---
bool(true)
bool(true)
bool(true)
ID=10: NUM_9_0=123456789, NUM_9_3=123456.789, NUM_9_6=123.456789, NUM_9_9=0.123456789
ID=11: NUM_9_0=999999999, NUM_9_3=999999.999, NUM_9_6=999.999999, NUM_9_9=0.999999999
ID=12: NUM_9_0=-987654321, NUM_9_3=-987654.321, NUM_9_6=-987.654321, NUM_9_9=-0.987654321

--- Test 3: BIGINT-backed NUMERIC ---
bool(true)
bool(true)
bool(true)
ID=20: NUM_18_0=%s, NUM_18_4=%s
ID=21: NUM_18_0=1, NUM_18_4=0.0001
ID=22: NUM_18_0=%s, NUM_18_4=%s

--- Test 4: DECIMAL types ---
bool(true)
bool(true)
bool(true)
ID=30: DEC_10_2=12345678.90, DEC_15_5=1234567890.12345, DEC_18_10=%s
ID=31: DEC_10_2=-99999999.99, DEC_15_5=-9999999999.99999, DEC_18_10=%s
ID=32: DEC_10_2=0.00, DEC_15_5=0.00000, DEC_18_10=%s

--- Test 5: Type coercion to scaled numeric ---
bool(true)
bool(true)
bool(true)
ID=40: NUM_9_3=12345.000, DEC_10_2=9999.00
ID=41: NUM_9_3=123.456, DEC_10_2=1234.56
ID=42: NUM_9_3=789.123, DEC_10_2=5678.90

--- Test 6: NULL values in scaled columns ---
bool(true)
bool(true)
ID=50: NUM_4_2=NULL, NUM_9_3=NULL, NUM_18_4=NULL, DEC_10_2=NULL
ID=51: NUM_4_2=12.34, NUM_9_3=NULL, NUM_18_4=1234.5678, DEC_10_2=NULL

--- Test 7: Boundary values ---
bool(true)
bool(true)
bool(true)
ID=60: NUM_4_0=9999, NUM_9_0=999999999, NUM_18_0=%s
ID=61: NUM_4_0=-9999, NUM_9_0=-999999999, NUM_18_0=%s
ID=62: NUM_4_0=1, NUM_9_0=1, NUM_18_0=1

--- Total rows inserted: %d ---

--- Cleanup ---

=== Test Complete ===
