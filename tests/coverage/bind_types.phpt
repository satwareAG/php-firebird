--TEST--
Coverage: Parameter binding type conversions (_php_fbird_bind)
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once __DIR__ . '/../skipif.inc';
?>
--FILE--
<?php
/**
 * Coverage test for parameter binding in fbird_query_bind.c
 *
 * Tests binding paths:
 * - Integer binding (LONG, SHORT, INT64)
 * - Float/Double binding
 * - String binding (VARYING, TEXT)
 * - Date/Time/Timestamp binding
 * - NULL value binding
 * - BLOB binding from string
 * - Numeric scale handling (DECIMAL/NUMERIC)
 */
require_once __DIR__ . '/../firebird.inc';

echo "=== Test: Parameter Binding Type Coverage ===\n";

$db = fbird_connect($test_base, $user, $password);
if (!$db) {
    die("Connection failed: " . fbird_errmsg() . "\n");
}

// Clean up
@fbird_query($db, 'DROP TABLE BIND_TEST');
@fbird_commit($db);

// Create table with various types
$create = "
CREATE TABLE BIND_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    INT_COL INTEGER,
    SMALLINT_COL SMALLINT,
    BIGINT_COL BIGINT,
    FLOAT_COL FLOAT,
    DOUBLE_COL DOUBLE PRECISION,
    DECIMAL_COL DECIMAL(10,2),
    NUMERIC_COL NUMERIC(15,4),
    CHAR_COL CHAR(20),
    VARCHAR_COL VARCHAR(100),
    DATE_COL DATE,
    TIME_COL TIME,
    TS_COL TIMESTAMP,
    BLOB_COL BLOB SUB_TYPE TEXT
)";
fbird_query($db, $create);
fbird_commit($db);
echo "Test table created\n";

// Test 1: Integer binding
echo "\n--- Test 1: Integer binding ---\n";
$stmt = fbird_prepare($db, "INSERT INTO BIND_TEST (ID, INT_COL, SMALLINT_COL, BIGINT_COL) VALUES (?, ?, ?, ?)");
$result = fbird_execute($stmt, 1, 2147483647, 32767, 9223372036854775807);
var_dump($result !== false);
fbird_commit($db);

// Verify
$r = fbird_query($db, "SELECT INT_COL, SMALLINT_COL, BIGINT_COL FROM BIND_TEST WHERE ID = 1");
$row = fbird_fetch_assoc($r);
echo "INT_COL: " . $row['INT_COL'] . "\n";
echo "SMALLINT_COL: " . $row['SMALLINT_COL'] . "\n";
echo "BIGINT_COL type: " . gettype($row['BIGINT_COL']) . "\n";
fbird_free_result($r);

// Test 2: Float/Double binding
echo "\n--- Test 2: Float/Double binding ---\n";
$stmt = fbird_prepare($db, "INSERT INTO BIND_TEST (ID, FLOAT_COL, DOUBLE_COL) VALUES (?, ?, ?)");
$result = fbird_execute($stmt, 2, 3.14159, 2.718281828459045);
var_dump($result !== false);
fbird_commit($db);

$r = fbird_query($db, "SELECT FLOAT_COL, DOUBLE_COL FROM BIND_TEST WHERE ID = 2");
$row = fbird_fetch_assoc($r);
echo "FLOAT_COL: " . round($row['FLOAT_COL'], 4) . "\n";
echo "DOUBLE_COL: " . round($row['DOUBLE_COL'], 10) . "\n";
fbird_free_result($r);

// Test 3: Decimal/Numeric binding (scale handling)
echo "\n--- Test 3: Decimal/Numeric scale binding ---\n";
$stmt = fbird_prepare($db, "INSERT INTO BIND_TEST (ID, DECIMAL_COL, NUMERIC_COL) VALUES (?, ?, ?)");
$result = fbird_execute($stmt, 3, 12345.67, 9876543.2109);
var_dump($result !== false);
fbird_commit($db);

$r = fbird_query($db, "SELECT DECIMAL_COL, NUMERIC_COL FROM BIND_TEST WHERE ID = 3");
$row = fbird_fetch_assoc($r);
echo "DECIMAL_COL: " . $row['DECIMAL_COL'] . "\n";
echo "NUMERIC_COL: " . $row['NUMERIC_COL'] . "\n";
fbird_free_result($r);

// Test 4: String binding (CHAR and VARCHAR)
echo "\n--- Test 4: String binding ---\n";
$stmt = fbird_prepare($db, "INSERT INTO BIND_TEST (ID, CHAR_COL, VARCHAR_COL) VALUES (?, ?, ?)");
$result = fbird_execute($stmt, 4, "Fixed width", "Variable length string with special chars: äöü");
var_dump($result !== false);
fbird_commit($db);

$r = fbird_query($db, "SELECT CHAR_COL, VARCHAR_COL FROM BIND_TEST WHERE ID = 4");
$row = fbird_fetch_assoc($r);
echo "CHAR_COL: '" . trim($row['CHAR_COL']) . "'\n";
echo "VARCHAR_COL contains UTF-8: " . (strpos($row['VARCHAR_COL'], 'äöü') !== false ? "YES" : "NO") . "\n";
fbird_free_result($r);

// Test 5: Date/Time/Timestamp binding
echo "\n--- Test 5: Date/Time/Timestamp binding ---\n";
$stmt = fbird_prepare($db, "INSERT INTO BIND_TEST (ID, DATE_COL, TIME_COL, TS_COL) VALUES (?, ?, ?, ?)");
$result = fbird_execute($stmt, 5, '2025-12-31', '23:59:59', '2025-06-15 12:30:45');
var_dump($result !== false);
fbird_commit($db);

$r = fbird_query($db, "SELECT DATE_COL, TIME_COL, TS_COL FROM BIND_TEST WHERE ID = 5");
$row = fbird_fetch_assoc($r);
echo "DATE_COL: " . $row['DATE_COL'] . "\n";
echo "TIME_COL: " . $row['TIME_COL'] . "\n";
echo "TS_COL contains 2025: " . (strpos($row['TS_COL'], '2025') !== false ? "YES" : "NO") . "\n";
fbird_free_result($r);

// Test 6: NULL binding
echo "\n--- Test 6: NULL binding ---\n";
$stmt = fbird_prepare($db, "INSERT INTO BIND_TEST (ID, INT_COL, VARCHAR_COL) VALUES (?, ?, ?)");
$result = fbird_execute($stmt, 6, null, null);
var_dump($result !== false);
fbird_commit($db);

$r = fbird_query($db, "SELECT INT_COL, VARCHAR_COL FROM BIND_TEST WHERE ID = 6");
$row = fbird_fetch_assoc($r);
echo "INT_COL is NULL: " . (is_null($row['INT_COL']) ? "YES" : "NO") . "\n";
echo "VARCHAR_COL is NULL: " . (is_null($row['VARCHAR_COL']) ? "YES" : "NO") . "\n";
fbird_free_result($r);

// Test 7: BLOB binding from string
echo "\n--- Test 7: BLOB binding ---\n";
$stmt = fbird_prepare($db, "INSERT INTO BIND_TEST (ID, BLOB_COL) VALUES (?, ?)");
$blob_text = str_repeat("This is a test blob content. ", 100);
$result = fbird_execute($stmt, 7, $blob_text);
var_dump($result !== false);
fbird_commit($db);

$r = fbird_query($db, "SELECT BLOB_COL FROM BIND_TEST WHERE ID = 7");
$row = fbird_fetch_assoc($r, FBIRD_TEXT);
echo "BLOB length: " . strlen($row['BLOB_COL']) . "\n";
echo "BLOB starts with expected: " . (strpos($row['BLOB_COL'], 'This is a test') === 0 ? "YES" : "NO") . "\n";
fbird_free_result($r);

// Test 8: Type coercion - int as string
echo "\n--- Test 8: Type coercion ---\n";
$stmt = fbird_prepare($db, "INSERT INTO BIND_TEST (ID, INT_COL) VALUES (?, ?)");
$result = fbird_execute($stmt, 8, "12345");  // String that should become int
var_dump($result !== false);
fbird_commit($db);

$r = fbird_query($db, "SELECT INT_COL FROM BIND_TEST WHERE ID = 8");
$row = fbird_fetch_assoc($r);
echo "INT_COL from string '12345': " . $row['INT_COL'] . "\n";
fbird_free_result($r);

// Test 9: Multiple executes of same prepared statement
echo "\n--- Test 9: Reuse prepared statement ---\n";
$stmt = fbird_prepare($db, "INSERT INTO BIND_TEST (ID, VARCHAR_COL) VALUES (?, ?)");
for ($i = 100; $i < 105; $i++) {
    fbird_execute($stmt, $i, "Row $i");
}
fbird_commit($db);

$r = fbird_query($db, "SELECT COUNT(*) AS CNT FROM BIND_TEST WHERE ID >= 100");
$row = fbird_fetch_assoc($r);
echo "Rows inserted via reused stmt: " . $row['CNT'] . "\n";
fbird_free_result($r);

// Test 10: Binding with negative numbers
echo "\n--- Test 10: Negative number binding ---\n";
$stmt = fbird_prepare($db, "INSERT INTO BIND_TEST (ID, INT_COL, DECIMAL_COL) VALUES (?, ?, ?)");
$result = fbird_execute($stmt, 10, -999999, -12345.67);
var_dump($result !== false);
fbird_commit($db);

$r = fbird_query($db, "SELECT INT_COL, DECIMAL_COL FROM BIND_TEST WHERE ID = 10");
$row = fbird_fetch_assoc($r);
echo "Negative INT_COL: " . $row['INT_COL'] . "\n";
echo "Negative DECIMAL_COL: " . $row['DECIMAL_COL'] . "\n";
fbird_free_result($r);

// Cleanup
echo "\n--- Cleanup ---\n";
@fbird_query($db, 'DROP TABLE BIND_TEST');
@fbird_commit($db);
fbird_close($db);

echo "\n=== Test Complete ===\n";
?>
--EXPECTF--
=== Test: Parameter Binding Type Coverage ===
Test table created

--- Test 1: Integer binding ---
bool(true)
INT_COL: 2147483647
SMALLINT_COL: 32767
BIGINT_COL type: %s

--- Test 2: Float/Double binding ---
bool(true)
FLOAT_COL: 3.1416
DOUBLE_COL: 2.7182818285

--- Test 3: Decimal/Numeric scale binding ---
bool(true)
DECIMAL_COL: 12345.67
NUMERIC_COL: 9876543.2109

--- Test 4: String binding ---
bool(true)
CHAR_COL: 'Fixed width'
VARCHAR_COL contains UTF-8: YES

--- Test 5: Date/Time/Timestamp binding ---
bool(true)
DATE_COL: 2025-12-31
TIME_COL: 23:59:59
TS_COL contains 2025: YES

--- Test 6: NULL binding ---
bool(true)
INT_COL is NULL: YES
VARCHAR_COL is NULL: YES

--- Test 7: BLOB binding ---
bool(true)
BLOB length: %d
BLOB starts with expected: YES

--- Test 8: Type coercion ---
bool(true)
INT_COL from string '12345': 12345

--- Test 9: Reuse prepared statement ---
Rows inserted via reused stmt: 5

--- Test 10: Negative number binding ---
bool(true)
Negative INT_COL: -999999
Negative DECIMAL_COL: -12345.67

--- Cleanup ---

=== Test Complete ===
