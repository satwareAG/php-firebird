--TEST--
pdo_fbird: placeholder parsing edge cases (colons in strings, comments)
--SKIPIF--
<?php
if (!extension_loaded('pdo')) die('skip PDO not available');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not loaded');
require_once __DIR__ . '/pdo_fbird.inc';
try { pdo_fbird_connect(); } catch (Throwable $e) { die('skip cannot connect: ' . $e->getMessage()); }
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();

// Colon inside a string literal should NOT be treated as a named param
$stmt = $pdo->query("SELECT 'hello:world' AS val FROM RDB\$DATABASE");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "String colon: " . trim($row['VAL']) . "\n";

// CAST syntax should work fine
$stmt = $pdo->query("SELECT CAST(123 AS VARCHAR(10)) AS val FROM RDB\$DATABASE");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "Cast: " . trim($row['VAL']) . "\n";

// Colon in a WHERE clause string comparison
$pdo->exec("RECREATE TABLE ipm_test (val VARCHAR(50))");
$pdo->exec("INSERT INTO ipm_test VALUES ('test:value')");
$stmt = $pdo->query("SELECT val FROM ipm_test WHERE val = 'test:value'");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "Where colon: " . trim($row['VAL']) . "\n";

$pdo->exec("DROP TABLE ipm_test");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
String colon: hello:world
Cast: 123
Where colon: test:value
Done

--CLEAN--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();
@$pdo->exec("DROP TABLE ipm_test");
unset($pdo);
?>
