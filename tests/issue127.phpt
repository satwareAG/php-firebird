--TEST--
Issue #127: Clean EOF for fetch functions (No warnings)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

$conn = fbird_connect($test_base, $user, $password);

echo "Fetching all rows from a small table...\n";
$q = fbird_query($conn, "SELECT * FROM test1");

while ($row = fbird_fetch_assoc($q)) {
    // Just fetch until end
}

echo "Result of next fbird_fetch_assoc() call after EOF:\n";
$row = fbird_fetch_assoc($q);
if ($row === false) {
    echo "OK: Result is false\n";
} else {
    echo "FAIL: Result is not false: " . var_export($row, true) . "\n";
}

fbird_close($conn);
?>
--EXPECTF--
Fetching all rows from a small table...
Result of next fbird_fetch_assoc() call after EOF:
OK: Result is false
