--TEST--
Issue #23: Column alias deduplication works correctly with padded aliases (Firebird 4.0+)
--SKIPIF--
<?php
include("skipif.inc");
// Column alias padding behavior changed in Firebird 4.0
skip_if_fb_lt(4.0);
?>
--FILE--
<?php
/*
 * Test for Issue #23: Column alias deduplication fails on padded aliases
 *
 * Firebird 3.0+ may return CHAR-type column aliases with trailing space padding.
 * The extension must trim these before deduplication to ensure consistent keys.
 *
 * This test uses a deterministic CTE query to produce duplicate column names,
 * avoiding reliance on system table content which varies by Firebird version.
 *
 * Expected: "COL", "COL_01"
 * Bug behavior: "COL   ", "COL   _01" (trailing spaces before suffix)
 */

require_once('config.inc');
require("firebird.inc");

$db = fbird_connect($test_base);
if (!$db) {
    die("Could not connect to database\n");
}

// Deterministic CTE query that produces duplicate column names
// Using subqueries with the same alias guarantees duplicates
$sql = "WITH T1 AS (SELECT 1 AS COL FROM RDB\$DATABASE),
             T2 AS (SELECT 2 AS COL FROM RDB\$DATABASE)
        SELECT T1.COL, T2.COL FROM T1, T2";

$result = fbird_query($db, $sql);
if (!$result) {
    die("Query failed: " . fbird_errmsg() . "\n");
}

$row = fbird_fetch_assoc($result);
if (!$row) {
    die("No rows returned\n");
}

// Get all keys and check for trailing spaces
$keys = array_keys($row);
echo "Number of columns: " . count($keys) . "\n";

$has_trailing_spaces = false;
$has_dedup_suffix = false;

foreach ($keys as $key) {
    // Check for trailing spaces
    if (preg_match('/\s+$/', $key)) {
        $has_trailing_spaces = true;
        echo "FAIL: Key has trailing spaces: [$key]\n";
    }

    // Check for deduplication suffix
    if (preg_match('/_\d{2}$/', $key)) {
        $has_dedup_suffix = true;
    }

    // Output key for debugging (visible length and actual content)
    echo "Key: [" . $key . "] (length=" . strlen($key) . ")\n";
}

// Verify we have deduplication (proves duplicate columns were handled)
if ($has_dedup_suffix) {
    echo "OK: Deduplication suffix found\n";
} else {
    echo "FAIL: No deduplication suffix found (expected COL_01)\n";
}

// Overall result
if (!$has_trailing_spaces) {
    echo "SUCCESS: All keys are properly trimmed\n";
} else {
    echo "FAIL: Some keys have trailing spaces\n";
}

// Verify we can access keys without trailing spaces
if (isset($row['COL'])) {
    echo "OK: Key 'COL' accessible\n";
} else {
    echo "FAIL: Key 'COL' not found (might be padded)\n";
}

// Verify deduplication key is correct
if (isset($row['COL_01'])) {
    echo "OK: Key 'COL_01' accessible\n";
} else {
    echo "INFO: Key 'COL_01' not directly accessible\n";
}

fbird_free_result($result);
fbird_close($db);
echo "Test complete\n";
?>
--EXPECTF--
Number of columns: 2
Key: [COL] (length=%d)
Key: [COL_01] (length=%d)
OK: Deduplication suffix found
SUCCESS: All keys are properly trimmed
OK: Key 'COL' accessible
OK: Key 'COL_01' accessible
Test complete
