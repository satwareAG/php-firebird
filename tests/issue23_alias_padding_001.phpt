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
 * This test queries system tables that produce duplicate column names, verifying:
 * 1. Array keys are properly trimmed (no trailing spaces)
 * 2. Deduplication suffixes (_01, _02) are appended to trimmed names
 *
 * Expected: "RDB$FIELD_NAME", "RDB$FIELD_NAME_01"
 * Bug behavior: "RDB$FIELD_NAME   ", "RDB$FIELD_NAME   _01"
 */

require_once('config.inc');
require("firebird.inc");

$db = fbird_connect($test_base);
if (!$db) {
    die("Could not connect to database\n");
}

// Query that produces duplicate column names (F.RDB$FIELD_NAME and RF.RDB$FIELD_NAME)
$sql = "SELECT FIRST 1 F.RDB\$FIELD_NAME, RF.RDB\$FIELD_NAME
        FROM RDB\$FIELDS F
        JOIN RDB\$RELATION_FIELDS RF ON RF.RDB\$FIELD_SOURCE = F.RDB\$FIELD_NAME";

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
    echo "INFO: No deduplication needed (columns might have different names)\n";
}

// Overall result
if (!$has_trailing_spaces) {
    echo "SUCCESS: All keys are properly trimmed\n";
} else {
    echo "FAIL: Some keys have trailing spaces\n";
}

// Verify we can access keys without trailing spaces
if (isset($row['RDB$FIELD_NAME'])) {
    echo "OK: Key 'RDB\$FIELD_NAME' accessible\n";
} else {
    echo "FAIL: Key 'RDB\$FIELD_NAME' not found (might be padded)\n";
}

fbird_free_result($result);
fbird_close($db);
echo "Test complete\n";
?>
--EXPECTF--
Number of columns: 2
Key: [RDB$FIELD_NAME] (length=%d)
Key: [RDB$FIELD_NAME_01] (length=%d)
OK: Deduplication suffix found
SUCCESS: All keys are properly trimmed
OK: Key 'RDB$FIELD_NAME' accessible
Test complete
