--TEST--
Coverage: SQL_BOOLEAN binding from numeric/text strings
--EXTENSIONS--
firebird
--SKIPIF--
<?php
include __DIR__ . '/../skipif.inc';
skip_if_fb_lt(3);
?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);

fbird_query($dbh, "RECREATE TABLE BIND_BOOL_STR_TEST (
    ID INT PRIMARY KEY,
    VAL_BOOLEAN BOOLEAN
)");
fbird_commit($dbh);

$stmt = fbird_prepare($dbh, "INSERT INTO BIND_BOOL_STR_TEST (ID, VAL_BOOLEAN) VALUES (?, ?)");

$inputs = [
    [1, "0.0"],   // numeric string -> IS_DOUBLE -> false
    [2, "1.5"],   // numeric string -> IS_DOUBLE -> true
    [3, "0"],     // numeric string -> IS_LONG -> false
    [4, "2"],     // numeric string -> IS_LONG -> true
    [5, ""],      // empty string -> false
    [6, "true"],  // textual true
    [7, "false"], // textual false
];

foreach ($inputs as [$id, $value]) {
    if (fbird_execute($stmt, $id, $value) === false) {
        echo "FAIL\n";
        exit(1);
    }
}
fbird_commit($dbh);

$res = fbird_query(
    $dbh,
    "SELECT CASE WHEN VAL_BOOLEAN THEN 1 ELSE 0 END AS BOOL_INT
     FROM BIND_BOOL_STR_TEST
     ORDER BY ID"
);

$actual = [];
while ($row = fbird_fetch_assoc($res)) {
    $actual[] = (int) $row['BOOL_INT'];
}

$expected = [0, 1, 0, 1, 0, 1, 0];

if ($actual === $expected) {
    echo "OK\n";
} else {
    echo "FAIL\n";
    var_export($actual);
    echo "\n";
}

fbird_close($dbh);
?>
--EXPECT--
OK
--CLEAN--
<?php require_once __DIR__ . '/../clean.inc'; ?>
