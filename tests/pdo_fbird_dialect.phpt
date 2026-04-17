--TEST--
pdo_fbird: SQL dialect handling
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not available');
require_once __DIR__ . '/pdo_fbird.inc';
try { pdo_fbird_connect(); } catch (Throwable $e) { die('skip cannot connect: ' . $e->getMessage()); }
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';

/* Default connection (dialect 3) */
$pdo = pdo_fbird_connect();

/* Dialect 3 supports quoted identifiers */
$pdo->exec('RECREATE TABLE "dialect_Test" ("myCol" INTEGER)');
$pdo->exec('INSERT INTO "dialect_Test" ("myCol") VALUES (42)');
$stmt = $pdo->query('SELECT "myCol" FROM "dialect_Test"');
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "dialect3: " . $row['myCol'] . "\n";
$pdo->exec('DROP TABLE "dialect_Test"');

/* Verify server version is accessible */
$ver = $pdo->getAttribute(PDO::ATTR_SERVER_VERSION);
echo "server_version: " . (strlen($ver) > 0 ? "ok" : "empty") . "\n";

/* Verify driver name */
$drv = $pdo->getAttribute(PDO::ATTR_DRIVER_NAME);
echo "driver: $drv\n";

$pdo = null;
echo "Done\n";
?>
--EXPECT--
dialect3: 42
server_version: ok
driver: fbird
Done

--CLEAN--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]);
@$pdo->exec("DROP TABLE dialect_Test");
unset($pdo);
?>
