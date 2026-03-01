--TEST--
Coverage: SQL_LONG scaled bind rejects out-of-range value
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

$stmt = fbird_prepare(
    $dbh,
    "SELECT CAST(? AS NUMERIC(9,2)) FROM RDB\$DATABASE"
);

$ok = false;
$errText = '';

try {
    $result = @fbird_execute($stmt, "30000000.00");
    $errText = (string) fbird_errmsg();
    $ok = ($result === false) && (strpos($errText, 'scaled value out of range for LONG') !== false);
} catch (Throwable $e) {
    $errText = $e->getMessage();
    $ok = strpos($errText, 'scaled value out of range for LONG') !== false;
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