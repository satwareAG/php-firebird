--TEST--
Migration reliability: fbird_drop_table_force logic
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("interbase.inc");

$db = ibase_connect($test_base);

// 1. Setup
$table = "TEST_MIGRATION_FORCE";
@ibase_query($db, "DROP TABLE $table");
ibase_query($db, "CREATE TABLE $table (ID INT)");
ibase_query($db, "INSERT INTO $table VALUES (1)");
ibase_commit($db);

// 2. Create a blocker
// Open a second connection
$db2 = ibase_connect($test_base);
// Use a transaction that locks the table logic.
// Opening a cursor on the table usually prevents DROP.
$trans2 = ibase_trans($db2, IBASE_READ | IBASE_CONCURRENCY); // Snapshot
$cursor = ibase_query($trans2, "SELECT * FROM $table");
// Cursor is open.

echo "Attempting standard DROP TABLE (expect failure)...\n";
// Using a new transaction on db1
$trans1 = ibase_trans($db);
// Suppress error output to check return value
$res = @ibase_query($trans1, "DROP TABLE $table");

if ($res === false) {
    echo "Standard DROP failed as expected (Object in use).\n";
    echo "Error: " . ibase_errmsg() . "\n";
} else {
    echo "Standard DROP succeeded unexpectedly!\n";
}
ibase_commit($trans1);

// 3. Inspect blockers
echo "Inspecting blockers...\n";
$blockers = fbird_list_table_blockers($db, $table);
if (count($blockers) > 0) {
    echo "Found " . count($blockers) . " blockers.\n";
    // verify info
    if (isset($blockers[0]['attachment_id']) && isset($blockers[0]['user'])) {
        echo "Blocker info valid.\n";
    }
} else {
    echo "No blockers found? (Unexpected)\n";
    var_dump($blockers);
}

// 4. Force Drop
echo " executing fbird_drop_table_force...\n";
$result = fbird_drop_table_force($db, $table);

if ($result) {
    echo "Force Drop returned TRUE.\n";
} else {
    echo "Force Drop returned FALSE.\n";
    echo "Error: " . ibase_errmsg() . "\n";
}

// Verify table is gone
// Reconnect to verify (blocker connection might be dead now)
$db3 = ibase_connect($test_base);
// Query system tables
$check = ibase_query($db3, "SELECT RDB$RELATION_NAME FROM RDB$RELATIONS WHERE RDB$RELATION_NAME = '$table'");
if ($row = ibase_fetch_row($check)) {
    echo "Table still exists!\n";
} else {
    echo "Table gone confirmed.\n";
}

// Clean up
ibase_close($db);
// $db2 connection might be invalid now
@ibase_close($db2);
@ibase_close($db3);

?>
--EXPECTF--
Attempting standard DROP TABLE (expect failure)...
Standard DROP failed as expected (Object in use).
Error: %s
Inspecting blockers...
Found %d blockers.
Blocker info valid.
 executing fbird_drop_table_force...
Force Drop returned TRUE.
Table gone confirmed.
