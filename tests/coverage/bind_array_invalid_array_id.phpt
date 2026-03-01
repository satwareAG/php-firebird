--TEST--
Coverage: SQL_ARRAY binding rejects non-array scalar with invalid array ID
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

fbird_query($dbh, "RECREATE TABLE BIND_ARRAY_ID_TEST (
    ID INT PRIMARY KEY,
    V_INT INTEGER[10]
)");
fbird_commit($dbh);

$stmt = fbird_prepare(
    $dbh,
    "INSERT INTO BIND_ARRAY_ID_TEST (ID, V_INT) VALUES (?, ?)"
);

$ok = false;
$errText = '';

try {
    $result = @fbird_execute($stmt, 1, "not-an-array-id");
    $errText = (string) fbird_errmsg();
    $ok = ($result === false) && (strpos($errText, 'invalid array ID') !== false);
} catch (Throwable $e) {
    $errText = $e->getMessage();
    $ok = strpos($errText, 'invalid array ID') !== false;
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