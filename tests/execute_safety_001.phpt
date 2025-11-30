--TEST--
API Safety: fbird_execute_statement vs fbird_execute_query
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("interbase.inc");

$db = ibase_connect($test_base);
$trans = ibase_trans($db);

// 1. Correct Usage of fbird_execute_statement (DDL)
echo "1. DDL via execute_statement...\n";
$res = fbird_execute_statement($trans, "recreate table test_exec_safety (id int)");
var_dump($res); // int(0)

// 2. Correct Usage of fbird_execute_statement (DML)
echo "2. DML via execute_statement...\n";
$res = fbird_execute_statement($trans, "insert into test_exec_safety values (1)");
var_dump($res); // int(1) affected row

// 3. Incorrect Usage: SELECT via execute_statement (Expect Error)
echo "3. SELECT via execute_statement (Should fail)...\n";
try {
    $res = fbird_execute_statement($trans, "select * from test_exec_safety");
    echo "Unexpected success!\n";
} catch (Throwable $e) {
    echo "Caught expected error: " . $e->getMessage() . "\n";
}

// 4. Correct Usage of fbird_execute_query (SELECT)
echo "4. SELECT via execute_query...\n";
$res = fbird_execute_query($trans, "select * from test_exec_safety");
var_dump(is_resource($res)); // bool(true)
$row = ibase_fetch_row($res);
var_dump($row[0]); // int(1)
ibase_free_result($res);

// 5. Incorrect Usage: DML via execute_query (Expect Error)
echo "5. DML via execute_query (Should fail)...\n";
try {
    $res = fbird_execute_query($trans, "insert into test_exec_safety values (2)");
    echo "Unexpected success!\n";
} catch (Throwable $e) {
    echo "Caught expected error: " . $e->getMessage() . "\n";
}

// 6. Autonomous Transaction
echo "6. Autonomous Transaction...\n";
// This should commit automatically
fbird_execute_auto($db, "insert into test_exec_safety values (3)");

// Check in our main transaction (need commit to see it? No, main trans started before auto?
// If main is READ_COMMITTED, it should see committed data.)
ibase_commit($trans);
$trans = ibase_trans($db);
$res = fbird_execute_query($trans, "select count(*) from test_exec_safety");
$row = ibase_fetch_row($res);
echo "Count after auto: " . $row[0] . "\n";

ibase_close($db);
?>
--EXPECTF--
1. DDL via execute_statement...
int(0)
2. DML via execute_statement...
long(1)
3. SELECT via execute_statement (Should fail)...
Caught expected error: fbird_execute_statement expects a DML/DDL statement, but SELECT was executed. Use fbird_execute_query().
4. SELECT via execute_query...
bool(true)
int(1)
5. DML via execute_query (Should fail)...
Caught expected error: fbird_execute_query expects a SELECT or RETURNING statement.
6. Autonomous Transaction...
Count after auto: 3
