--TEST--
Coverage: SQL_ARRAY binding succeeds for UPDATE statement with array parameter
--EXTENSIONS--
firebird
--SKIPIF--
<?php
include __DIR__ . '/../skipif.inc';
?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);

fbird_query($dbh, "RECREATE TABLE BIND_ARRAY_CDT_UPDATE_TEST (
    ID INT PRIMARY KEY,
    V_INT INTEGER[2]
)");
fbird_commit($dbh);

fbird_query($dbh, "INSERT INTO BIND_ARRAY_CDT_UPDATE_TEST (ID, V_INT) VALUES (1, NULL)");
fbird_commit($dbh);

$stmt = fbird_prepare(
    $dbh,
    "UPDATE BIND_ARRAY_CDT_UPDATE_TEST SET V_INT = ? WHERE ID = ?"
);

$ok = false;
$errText = '';

try {
    $result = @fbird_execute($stmt, [1, 2], 1);
    $errText = (string) fbird_errmsg();
    $ok = ($result !== false);
} catch (Throwable $e) {
    $errText = $e->getMessage();
    $ok = false;
}

if ($ok) {
    echo "OK\n";
} else {
    echo "FAIL\n";
    var_dump($errText);
}

fbird_close($dbh);
?>
--EXPECT--
OK
--CLEAN--
<?php require_once __DIR__ . '/../clean.inc'; ?>
