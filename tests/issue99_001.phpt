--TEST--
Issue #99: CHAR fields should report as CHAR, not VARCHAR
--SKIPIF--
<?php
include("skipif.inc");
?>
--FILE--
<?php
/**
 * Upstream Issue #99: fbird_field_info() returns "VARCHAR" instead of "CHAR" for CHAR fields
 * https://github.com/FirebirdSQL/php-firebird/issues/99
 *
 * This test verifies that CHAR fields are correctly identified as "CHAR" type
 * in the fbird_field_info() output.
 */

require("firebird.inc");

// Connect
$db = fbird_connect($test_base);
if (!$db) {
    die("Cannot connect: " . fbird_errmsg());
}

$table_name = "ISSUE99_CHAR_TYPE";

// Clean up if exists
@fbird_query($db, "DROP TABLE $table_name");
fbird_commit($db);

// Create test table with CHAR field (exact reproduction from issue)
$sql = "CREATE TABLE $table_name (CHAR_FIXED CHAR(10) DEFAULT 'ABCDE')";
$result = fbird_query($db, $sql);
if (!$result) {
    die("Cannot create table: " . fbird_errmsg());
}
fbird_commit($db);

// Prepare the SELECT query
$q = fbird_prepare($db, "SELECT * FROM $table_name");
if (!$q) {
    die("Cannot prepare: " . fbird_errmsg());
}

// Get field info for the CHAR field
$info = fbird_field_info($q, 0);

echo "Field name: " . $info["name"] . "\n";
echo "Field type: " . $info["type"] . "\n";
echo "Field length: " . $info["length"] . "\n";

// Verification: Type MUST be "CHAR", not "VARCHAR"
if ($info["type"] === "CHAR") {
    echo "TEST PASSED: CHAR field correctly reports as CHAR\n";
} else {
    echo "TEST FAILED: Expected 'CHAR', got '{$info["type"]}'\n";
}

// Additional test: Explicit comparison test table with both CHAR and VARCHAR
@fbird_query($db, "DROP TABLE ISSUE99_COMPARISON");
fbird_commit($db);

fbird_query($db, "CREATE TABLE ISSUE99_COMPARISON (
    CHAR_COL CHAR(20),
    VARCHAR_COL VARCHAR(20)
)");
fbird_commit($db);

$q2 = fbird_prepare($db, "SELECT CHAR_COL, VARCHAR_COL FROM ISSUE99_COMPARISON");

$char_info = fbird_field_info($q2, 0);
$varchar_info = fbird_field_info($q2, 1);

echo "\n--- Comparison Test ---\n";
echo "CHAR_COL type: " . $char_info["type"] . "\n";
echo "VARCHAR_COL type: " . $varchar_info["type"] . "\n";

if ($char_info["type"] === "CHAR" && $varchar_info["type"] === "VARCHAR") {
    echo "COMPARISON TEST PASSED: Types are correctly differentiated\n";
} else {
    echo "COMPARISON TEST FAILED: Type differentiation broken\n";
}

// Cleanup
fbird_free_query($q);
fbird_free_query($q2);
fbird_query($db, "DROP TABLE $table_name");
fbird_query($db, "DROP TABLE ISSUE99_COMPARISON");
fbird_commit($db);
fbird_close($db);

echo "\nDone.\n";
?>
--EXPECT--
Field name: CHAR_FIXED
Field type: CHAR
Field length: 10
TEST PASSED: CHAR field correctly reports as CHAR

--- Comparison Test ---
CHAR_COL type: CHAR
VARCHAR_COL type: VARCHAR
COMPARISON TEST PASSED: Types are correctly differentiated

Done.
