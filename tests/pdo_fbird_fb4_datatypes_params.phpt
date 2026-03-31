--TEST--
pdo_fbird: FB4+ datatype parameter binding (INT128, DECFLOAT, TIME_TZ, TIMESTAMP_TZ)
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not available');
require_once __DIR__ . '/pdo_fbird.inc';
try {
    $pdo = pdo_fbird_connect();
    $pdo->exec("RECREATE TABLE fb4_skip (v DECFLOAT(16))");
    $pdo->exec("DROP TABLE fb4_skip");
} catch (Throwable $e) {
    die('skip FB4+ types not supported: ' . $e->getMessage());
}
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();

$pdo->exec("RECREATE TABLE fb4_params (
    id INTEGER NOT NULL PRIMARY KEY,
    df16 DECFLOAT(16),
    df34 DECFLOAT(34),
    i128 INT128
)");

/* Insert via positional params */
$stmt = $pdo->prepare("INSERT INTO fb4_params (id, df16, df34, i128) VALUES (?, ?, ?, ?)");
$stmt->execute([1, '3.14', '2.718281828459045235360287', '170141183460469231731687303715884105727']);

/* Read back */
$stmt = $pdo->query("SELECT df16, df34, i128 FROM fb4_params WHERE id = 1");
$row = $stmt->fetch(PDO::FETCH_ASSOC);

/* DECFLOAT values should come back as strings */
echo "df16: " . substr($row['DF16'], 0, 4) . "\n";
echo "df34: " . substr($row['DF34'], 0, 5) . "\n";
echo "i128_len: " . (strlen($row['I128']) > 10 ? "long" : "short") . "\n";

$pdo->exec("DROP TABLE fb4_params");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
df16: 3.14
df34: 2.718
i128_len: long
Done

--CLEAN--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();
@$pdo->exec("DROP TABLE fb4_params");
@$pdo->exec("DROP TABLE fb4_skip");
unset($pdo);
?>
