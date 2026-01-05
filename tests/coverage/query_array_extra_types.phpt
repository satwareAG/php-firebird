--TEST--
Coverage: Array binding for DATE, TIME, TIMESTAMP, FLOAT, DOUBLE
--SKIPIF--
<?php
if (!extension_loaded("firebird")) print "skip";
require __DIR__ . '/../firebird.inc';
if (!@fbird_connect($test_base, $user, $password)) die("skip cannot connect");
?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base, $user, $password);

$tableName = "TEST_ARRAY_TYPES";
// Suppress error if table doesn't exist (though init_db creates fresh DB usually)
@fbird_query($dbh, "DROP TABLE $tableName");

// Firebird 3.0+ supports BOOLEAN, but we test standard types first
$createTable = <<<SQL
CREATE TABLE $tableName (
    ID INT NOT NULL PRIMARY KEY,
    VAL_FLOAT FLOAT[3],
    VAL_DOUBLE DOUBLE PRECISION[3],
    VAL_DATE DATE[3],
    VAL_TIME TIME[3],
    VAL_TIMESTAMP TIMESTAMP[3]
)
SQL;

if (!fbird_query($dbh, $createTable)) {
    die("Failed to create table: " . fbird_errmsg());
}

fbird_commit($dbh);

// Data to insert
$floats = [1.1, 2.2, 3.3];
$doubles = [1.1111111, 2.2222222, 3.3333333];
$dates = ["2023-01-01", "2023-01-02", "2023-01-03"];
$times = ["12:00:00", "13:00:00", "14:00:00"];
$timestamps = ["2023-01-01 12:00:00", "2023-01-02 13:00:00", "2023-01-03 14:00:00"];

$sql = "INSERT INTO $tableName (ID, VAL_FLOAT, VAL_DOUBLE, VAL_DATE, VAL_TIME, VAL_TIMESTAMP) VALUES (?, ?, ?, ?, ?, ?)";

$res = fbird_query($dbh, $sql, 1, $floats, $doubles, $dates, $times, $timestamps);

if (!$res) {
    echo "Insert failed: " . fbird_errmsg() . "\n";
} else {
    echo "Insert successful\n";
}

fbird_commit($dbh);

$sel = fbird_query($dbh, "SELECT * FROM $tableName");
// Must pass FBIRD_FETCH_ARRAYS to decode array columns
$row = fbird_fetch_assoc($sel, FBIRD_FETCH_ARRAYS);

// Float precision display varies, verify type and count
var_dump(count($row['VAL_FLOAT']));
echo gettype($row['VAL_FLOAT'][1]) . "\n";

var_dump(count($row['VAL_DOUBLE']));
echo gettype($row['VAL_DOUBLE'][1]) . "\n";

print_r($row['VAL_DATE']);
print_r($row['VAL_TIME']);
print_r($row['VAL_TIMESTAMP']);

fbird_free_query($sel);
fbird_close($dbh);
?>
--EXPECTF--
Insert successful
int(3)
double
int(3)
double
Array
(
    [1] => 2023-01-01
    [2] => 2023-01-02
    [3] => 2023-01-03
)
Array
(
    [1] => 12:00:00
    [2] => 13:00:00
    [3] => 14:00:00
)
Array
(
    [1] => 2023-01-01 12:00:00
    [2] => 2023-01-02 13:00:00
    [3] => 2023-01-03 14:00:00
)
