--TEST--
pdo_fbird: basic query and fetch
--SKIPIF--
<?php
if (!extension_loaded('pdo')) die('skip PDO not available');
if (!extension_loaded('pdo_fbird')) die('skip pdo_fbird not loaded');
if (!extension_loaded('firebird')) die('skip firebird not loaded');
require_once __DIR__ . '/../../tests/firebird.inc';
if (!@fbird_connect($test_base, $user, $password)) die('skip cannot connect to Firebird');
?>
--FILE--
<?php
require_once __DIR__ . '/../../tests/firebird.inc';

if (strpos($test_base, ':') !== false) {
    [$dsn_host, $dsn_db] = explode(':', $test_base, 2);
} else { $dsn_host = 'localhost'; $dsn_db = $test_base; }

$pdo = new PDO("fbird:host={$dsn_host};dbname={$dsn_db}", $user, $password,
    [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);

$stmt = $pdo->query("SELECT 1 AS VAL FROM RDB\$DATABASE");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "VAL=" . $row['VAL'] . "\n";
$stmt = null;

$stmt2 = $pdo->query("SELECT 'hello' AS GREETING FROM RDB\$DATABASE");
$row2 = $stmt2->fetch(PDO::FETCH_ASSOC);
echo "GREETING=" . trim($row2['GREETING']) . "\n";
$stmt2 = null;

$pdo = null;
echo "ok\n";
?>
--EXPECT--
VAL=1
GREETING=hello
ok
