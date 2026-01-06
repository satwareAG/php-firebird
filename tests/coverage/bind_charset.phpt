--TEST--
Coverage: Character set handling (fbird_query_bind.c)
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once __DIR__ . '/../skipif.inc';
?>
--FILE--
<?php
/**
 * Coverage test for character set handling in fbird_query_bind.c
 *
 * Tests charset paths:
 * - UTF8 strings
 * - ISO8859_1 (Latin-1)
 * - Multi-byte characters
 * - Character length vs byte length
 *
 * Target: Cover charset conversion paths
 */
require_once __DIR__ . '/../firebird.inc';

echo "=== Test: Character Set Handling ===\n";

$db = fbird_connect($test_base, $user, $password);
if (!$db) {
    die("Connection failed: " . fbird_errmsg() . "\n");
}

// Clean up
@fbird_query($db, 'DROP TABLE CHARSET_TEST');
@fbird_commit($db);

// Create test table with UTF8 columns
$create = "
CREATE TABLE CHARSET_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    UTF8_VAR VARCHAR(100) CHARACTER SET UTF8,
    UTF8_CHAR CHAR(50) CHARACTER SET UTF8,
    ASCII_VAR VARCHAR(100) CHARACTER SET ASCII,
    NONE_VAR VARCHAR(100) CHARACTER SET NONE
)";
fbird_query($db, $create);
fbird_commit($db);
echo "Test table created\n";

// Test 1: Basic ASCII
echo "\n--- Test 1: ASCII strings ---\n";
$stmt = fbird_prepare($db, "INSERT INTO CHARSET_TEST (ID, UTF8_VAR, ASCII_VAR) VALUES (?, ?, ?)");

fbird_execute($stmt, 1, 'Hello World', 'Hello World');
fbird_execute($stmt, 2, 'Testing 123', 'Testing 123');
fbird_execute($stmt, 3, '!@#$%^&*()', '!@#$%^&*()');
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, UTF8_VAR, ASCII_VAR FROM CHARSET_TEST WHERE ID <= 3 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    echo "ID={$row['ID']}: UTF8='{$row['UTF8_VAR']}', ASCII='{$row['ASCII_VAR']}'\n";
}
fbird_free_result($r);

// Test 2: UTF-8 multi-byte characters
echo "\n--- Test 2: UTF-8 multi-byte characters ---\n";
$stmt = fbird_prepare($db, "INSERT INTO CHARSET_TEST (ID, UTF8_VAR) VALUES (?, ?)");

$utf8_strings = [
    10 => 'Äpfel und Öl',         // German umlauts
    11 => 'Ça va bien',           // French cedilla
    12 => 'Año nuevo',            // Spanish tilde
    13 => 'Привет мир',           // Russian
    14 => '你好世界',              // Chinese
    15 => '🎉🚀💻',               // Emoji (4-byte UTF-8)
];

foreach ($utf8_strings as $id => $str) {
    $result = @fbird_execute($stmt, $id, $str);
    if ($result === false) {
        echo "ID=$id: Failed (expected for some edge cases)\n";
    } else {
        echo "ID=$id: Inserted (" . strlen($str) . " bytes, " . mb_strlen($str, 'UTF-8') . " chars)\n";
    }
}
fbird_commit($db);

// Fetch and verify
$r = fbird_query($db, "SELECT ID, UTF8_VAR, OCTET_LENGTH(UTF8_VAR) AS BYTES, CHAR_LENGTH(UTF8_VAR) AS CHARS FROM CHARSET_TEST WHERE ID >= 10 AND ID < 20 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    echo "ID={$row['ID']}: '{$row['UTF8_VAR']}' ({$row['BYTES']} bytes, {$row['CHARS']} chars)\n";
}
fbird_free_result($r);

// Test 3: CHAR with padding (UTF8)
echo "\n--- Test 3: CHAR column with UTF-8 ---\n";
$stmt = fbird_prepare($db, "INSERT INTO CHARSET_TEST (ID, UTF8_CHAR) VALUES (?, ?)");

fbird_execute($stmt, 20, 'Short');
fbird_execute($stmt, 21, 'Längerer Text');
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, UTF8_CHAR, CHAR_LENGTH(UTF8_CHAR) AS LEN FROM CHARSET_TEST WHERE ID >= 20 AND ID < 30 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    // Trim for display
    $trimmed = rtrim($row['UTF8_CHAR']);
    echo "ID={$row['ID']}: Stored len={$row['LEN']}, trimmed='{$trimmed}'\n";
}
fbird_free_result($r);

// Test 4: NONE charset (binary-safe)
echo "\n--- Test 4: NONE charset (binary-like) ---\n";
$stmt = fbird_prepare($db, "INSERT INTO CHARSET_TEST (ID, NONE_VAR) VALUES (?, ?)");

fbird_execute($stmt, 30, 'Plain text');
fbird_execute($stmt, 31, "With\ttabs\tand\nnewlines");
fbird_execute($stmt, 32, "\x00\x01\x02");  // Binary values might fail
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, NONE_VAR, OCTET_LENGTH(NONE_VAR) AS BYTES FROM CHARSET_TEST WHERE ID >= 30 AND ID < 40 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    $display = str_replace(["\t", "\n", "\r"], ['\\t', '\\n', '\\r'], $row['NONE_VAR']);
    echo "ID={$row['ID']}: '{$display}' ({$row['BYTES']} bytes)\n";
}
fbird_free_result($r);

// Test 5: Mixed inserts
echo "\n--- Test 5: Mixed charset columns ---\n";
$stmt = fbird_prepare($db, "INSERT INTO CHARSET_TEST (ID, UTF8_VAR, UTF8_CHAR, ASCII_VAR, NONE_VAR) VALUES (?, ?, ?, ?, ?)");

fbird_execute($stmt, 40, 'UTF8 content über', 'Padded CHAR über', 'ASCII only', 'None data');
fbird_commit($db);

$r = fbird_query($db, "SELECT * FROM CHARSET_TEST WHERE ID = 40");
$row = fbird_fetch_assoc($r);
echo "ID={$row['ID']}:\n";
echo "  UTF8_VAR: '{$row['UTF8_VAR']}'\n";
echo "  UTF8_CHAR: '" . rtrim($row['UTF8_CHAR']) . "'\n";
echo "  ASCII_VAR: '{$row['ASCII_VAR']}'\n";
echo "  NONE_VAR: '{$row['NONE_VAR']}'\n";
fbird_free_result($r);

// Test 6: NULL handling across charsets
echo "\n--- Test 6: NULL values in charset columns ---\n";
$stmt = fbird_prepare($db, "INSERT INTO CHARSET_TEST (ID, UTF8_VAR, ASCII_VAR) VALUES (?, ?, ?)");
fbird_execute($stmt, 50, null, null);
fbird_execute($stmt, 51, 'Not null', null);
fbird_execute($stmt, 52, null, 'Not null');
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, UTF8_VAR IS NULL AS U_NULL, ASCII_VAR IS NULL AS A_NULL FROM CHARSET_TEST WHERE ID >= 50 AND ID < 60 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    echo "ID={$row['ID']}: UTF8 NULL=" . ($row['U_NULL'] ? 'Y' : 'N') . ", ASCII NULL=" . ($row['A_NULL'] ? 'Y' : 'N') . "\n";
}
fbird_free_result($r);

// Test 7: Long UTF-8 strings
echo "\n--- Test 7: Long UTF-8 strings ---\n";
$stmt = fbird_prepare($db, "INSERT INTO CHARSET_TEST (ID, UTF8_VAR) VALUES (?, ?)");

$long_utf8 = str_repeat('äöü', 30);  // 90 characters, 180 bytes
fbird_execute($stmt, 60, $long_utf8);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, OCTET_LENGTH(UTF8_VAR) AS BYTES, CHAR_LENGTH(UTF8_VAR) AS CHARS FROM CHARSET_TEST WHERE ID = 60");
$row = fbird_fetch_assoc($r);
echo "ID={$row['ID']}: {$row['BYTES']} bytes, {$row['CHARS']} chars\n";
fbird_free_result($r);

// Final count
$r = fbird_query($db, "SELECT COUNT(*) AS CNT FROM CHARSET_TEST");
$row = fbird_fetch_assoc($r);
echo "\n--- Total rows: " . $row['CNT'] . " ---\n";
fbird_free_result($r);

// Cleanup
echo "\n--- Cleanup ---\n";
@fbird_query($db, 'DROP TABLE CHARSET_TEST');
@fbird_commit($db);
fbird_close($db);

echo "\n=== Test Complete ===\n";
?>
--EXPECTF--
=== Test: Character Set Handling ===
Test table created

--- Test 1: ASCII strings ---
ID=1: UTF8='Hello World', ASCII='Hello World'
ID=2: UTF8='Testing 123', ASCII='Testing 123'
ID=3: UTF8='!@#$%^&*()', ASCII='!@#$%^&*()'

--- Test 2: UTF-8 multi-byte characters ---
ID=10: Inserted (%d bytes, %d chars)
ID=11: Inserted (%d bytes, %d chars)
ID=12: Inserted (%d bytes, %d chars)
ID=13: Inserted (%d bytes, %d chars)
ID=14: Inserted (%d bytes, %d chars)
ID=15: %s
%A

--- Test 3: CHAR column with UTF-8 ---
ID=20: Stored len=%d, trimmed='Short'
ID=21: Stored len=%d, trimmed='Längerer Text'

--- Test 4: NONE charset (binary-like) ---
ID=30: 'Plain text' (10 bytes)
ID=31: 'With\ttabs\tand\nnewlines' (%d bytes)
%A

--- Test 5: Mixed charset columns ---
ID=40:
  UTF8_VAR: 'UTF8 content über'
  UTF8_CHAR: 'Padded CHAR über'
  ASCII_VAR: 'ASCII only'
  NONE_VAR: 'None data'

--- Test 6: NULL values in charset columns ---
ID=50: UTF8 NULL=Y, ASCII NULL=Y
ID=51: UTF8 NULL=N, ASCII NULL=Y
ID=52: UTF8 NULL=Y, ASCII NULL=N

--- Test 7: Long UTF-8 strings ---
ID=60: %d bytes, %d chars

--- Total rows: %d ---

--- Cleanup ---

=== Test Complete ===
