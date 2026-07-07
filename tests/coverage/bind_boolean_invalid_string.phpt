--TEST--
Coverage: SQL_BOOLEAN binding rejects non-boolean string
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

fbird_query($dbh, "RECREATE TABLE BIND_BOOL_INVALID_TEST (
    ID INT PRIMARY KEY,
    VAL_BOOLEAN BOOLEAN
)");
fbird_commit($dbh);

$stmt = fbird_prepare(
    $dbh,
    "INSERT INTO BIND_BOOL_INVALID_TEST (ID, VAL_BOOLEAN) VALUES (?, ?)"
);

$ok = false;
$errText = '';

try {
    $result = @fbird_execute($stmt, 1, "maybe");
    $errText = (string) fbird_errmsg();
    $ok = ($result === false) && (strpos($errText, 'cannot convert string to boolean') !== false);
} catch (Throwable $e) {
    $errText = $e->getMessage();
    $ok = strpos($errText, 'cannot convert string to boolean') !== false;
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
