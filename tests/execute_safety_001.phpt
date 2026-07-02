--TEST--
API Safety: fbird_execute_statement vs fbird_execute_query error validation
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

$db = fbird_connect($test_base);
$trans = fbird_trans($db);

// Helper to assert exception message
function assert_exception(callable $fn, string $expectedPart) {
    try {
        $fn();
        echo "Unexpected success!\n";
    } catch (Throwable $e) {
        if (strpos($e->getMessage(), $expectedPart) !== false) {
            echo "Caught expected error containing: '$expectedPart'\n";
        } else {
            echo "Caught UNEXPECTED error: " . $e->getMessage() . "\n";
        }
    }
}

// 1. Correct Usage of fbird_execute_statement (DDL)
echo "1. DDL via execute_statement...\n";
// Execute DDL. In Firebird, mixed DDL/DML in specific isolation levels or without commit
// can be invisible. We commit the DDL here to ensure the table is visible for DML.
$res = fbird_execute_statement($trans, "recreate table test_exec_safety (id int)");
var_dump($res); // int(0)
fbird_commit($trans);
$trans = fbird_trans($db); // Start new transaction for DML

// 2. Correct Usage of fbird_execute_statement (DML)
echo "2. DML via execute_statement...\n";
$res = fbird_execute_statement($trans, "insert into test_exec_safety values (1)");
var_dump($res); // int(1) affected row

// 3. Incorrect Usage: SELECT via execute_statement (Expect Error)
echo "3. SELECT via execute_statement (Should fail)...\n";
assert_exception(function() use ($trans) {
    fbird_execute_statement($trans, "select * from test_exec_safety");
}, "fbird_execute_statement expects a DML/DDL statement");

// 4. Correct Usage of fbird_execute_query (SELECT)
echo "4. SELECT via execute_query...\n";
$res = fbird_execute_query($trans, "select * from test_exec_safety");
var_dump($res instanceof \Firebird\ResultSet); // bool(true)
$row = fbird_fetch_row($res);
var_dump($row[0]); // int(1)
fbird_free_result($res);

// 5. Incorrect Usage: DML via execute_query (Expect Error)
echo "5. DML via execute_query (Should fail)...\n";
assert_exception(function() use ($trans) {
    fbird_execute_query($trans, "insert into test_exec_safety values (2)");
}, "fbird_execute_query expects a SELECT or RETURNING statement");

// 6. Autonomous Transaction
echo "6. Autonomous Transaction...\n";
// This should commit automatically
fbird_execute_auto($db, "insert into test_exec_safety values (3)");

// Check in our main transaction (need commit to see it? No, main trans started before auto?
// If main is READ_COMMITTED, it should see committed data.)
fbird_commit($trans);
$trans = fbird_trans($db);
$res = fbird_execute_query($trans, "select count(*) from test_exec_safety");
$row = fbird_fetch_row($res);
echo "Count after auto: " . $row[0] . "\n";

fbird_close($db);
?>
--EXPECTF--
1. DDL via execute_statement...
int(0)
2. DML via execute_statement...
int(1)
3. SELECT via execute_statement (Should fail)...
Caught expected error containing: 'fbird_execute_statement expects a DML/DDL statement'
4. SELECT via execute_query...
bool(true)
int(1)
5. DML via execute_query (Should fail)...
Caught expected error containing: 'fbird_execute_query expects a SELECT or RETURNING statement'
6. Autonomous Transaction...
Count after auto: 3
