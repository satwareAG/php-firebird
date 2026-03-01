--TEST--
Coverage: SQL_SHORT scaled bind rejects out-of-range value
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
    "SELECT CAST(? AS NUMERIC(4,2)) FROM RDB\$DATABASE"
);

$ok = false;
$errText = '';

try {
    $result = @fbird_execute($stmt, "400.00");
    $errText = (string) fbird_errmsg();
    $ok = ($result === false) && (strpos($errText, 'scaled value out of range for SHORT') !== false);
} catch (Throwable $e) {
    $errText = $e->getMessage();
    $ok = strpos($errText, 'scaled value out of range for SHORT') !== false;
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