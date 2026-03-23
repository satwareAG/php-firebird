--TEST--
pdo_fbird: FB 4+ datatypes (INT128, DECFLOAT)
--SKIPIF--
<?php
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not available');
require_once __DIR__ . '/pdo_fbird.inc';
try {
    $pdo = pdo_fbird_connect();
    $row = $pdo->query("SELECT RDB\$GET_CONTEXT('SYSTEM', 'ENGINE_VERSION') FROM RDB\$DATABASE")->fetch();
    if (version_compare($row[0], '4.0', '<')) die('skip Firebird 4.0+ required');
    $pdo = null;
} catch (Exception $e) {
    die('skip cannot connect: ' . $e->getMessage());
}
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

$pdo->exec("RECREATE TABLE fb4_types (
    id INTEGER NOT NULL PRIMARY KEY,
    val_int128 INT128,
    val_dec16 DECFLOAT(16),
    val_dec34 DECFLOAT(34)
)");

$pdo->exec("INSERT INTO fb4_types VALUES (1, 12345678901234567890, 3.14, 2.718281828459045235360287471352662)");
$pdo->exec("INSERT INTO fb4_types VALUES (2, -99999999999999999999, 1.23E10, -1.23456789012345678901234567890123E-20)");
$pdo->exec("INSERT INTO fb4_types VALUES (3, 0, 0, 0)");

$stmt = $pdo->query("SELECT val_int128, val_dec16, val_dec34 FROM fb4_types ORDER BY id");
while ($row = $stmt->fetch(PDO::FETCH_NUM)) {
    echo "INT128: " . $row[0] . "\n";
    echo "DEC16:  " . $row[1] . "\n";
    echo "DEC34:  " . $row[2] . "\n";
    echo "---\n";
}

$pdo->exec("DROP TABLE fb4_types");
$pdo = null;
echo "Done\n";
?>
--EXPECTF--
INT128: 12345678901234567890
DEC16:  3.14
DEC34:  2.718281828459045235360287471352662
---
INT128: -99999999999999999999
DEC16:  1.23E+10
DEC34:  -1.23456789012345678901234567890123%s
---
INT128: 0
DEC16:  0
DEC34:  0
---
Done
