--TEST--
Issue #131: fbird_commit_ret() resource lifecycle
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

$conn = fbird_connect($test_base, $user, $password);

echo "Starting transaction...\n";
$tr = fbird_trans($conn);

echo "Executing query in transaction...\n";
$q = fbird_query($tr, "SELECT * FROM RDB\$DATABASE");

echo "Checking result before commit_ret...\n";
/* Issue #296: fbird_query() now returns Firebird\ResultSet object, not resource */
if ($q instanceof \Firebird\ResultSet) echo "Result is a Firebird\ResultSet object\n";

echo "Calling fbird_commit_ret()...\n";
fbird_commit_ret($tr);

echo "Checking result AFTER commit_ret...\n";
// Result resource should remain valid according to Firebird API if we use commit_ret
if ($q instanceof \Firebird\ResultSet) {
    echo "OK: Result object still exists after commit_ret\n";
    $row = fbird_fetch_assoc($q);
    if ($row) {
        echo "OK: Fetched row after commit_ret\n";
    } else {
        echo "FAIL: Could not fetch row after commit_ret\n";
    }
} else {
    echo "Current behavior: Result resource invalid/closed after commit_ret\n";
}

fbird_close($conn);
?>
--EXPECTF--
Starting transaction...
Executing query in transaction...
Checking result before commit_ret...
Result is a Firebird\ResultSet object
Calling fbird_commit_ret()...
Checking result AFTER commit_ret...
OK: Result object still exists after commit_ret
OK: Fetched row after commit_ret
