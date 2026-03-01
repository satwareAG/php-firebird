--TEST--
Coverage: SQL_ARRAY binding fails with invalid array element conversion
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

fbird_query($dbh, "RECREATE TABLE BIND_ARRAY_BIND_FAIL_TEST (
    ID INT PRIMARY KEY,
    V_INT INTEGER[2]
)");
fbird_commit($dbh);

$stmt = fbird_prepare(
    $dbh,
    "INSERT INTO BIND_ARRAY_BIND_FAIL_TEST (ID, V_INT) VALUES (?, ?)"
);

$ok = false;
$errText = '';

try {
    $result = @fbird_execute(
        $stmt,
        1,
        [1 => 3000000000, 2 => 1]
    );
    $errText = (string) fbird_errmsg();
    $ok = ($result === false)
        && (strpos($errText, 'failed to bind array argument') !== false);
} catch (Throwable $e) {
    $errText = $e->getMessage();
    $ok = strpos($errText, 'failed to bind array argument') !== false;
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