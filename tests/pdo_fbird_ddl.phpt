--TEST--
pdo_fbird: DDL operations (CREATE, ALTER, DROP)
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
$pdo = pdo_fbird_connect();

/* CREATE TABLE */
$pdo->exec("RECREATE TABLE ddl_test (id INTEGER NOT NULL PRIMARY KEY, name VARCHAR(50))");
echo "created\n";

/* ALTER TABLE */
$pdo->exec("ALTER TABLE ddl_test ADD age INTEGER DEFAULT 0");
echo "altered\n";

/* INSERT to verify structure */
$pdo->exec("INSERT INTO ddl_test (id, name, age) VALUES (1, 'Alice', 30)");
$stmt = $pdo->query("SELECT id, name, age FROM ddl_test");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "row: {$row['ID']}-{$row['NAME']}-{$row['AGE']}\n";

/* DROP TABLE */
$pdo->exec("DROP TABLE ddl_test");
echo "dropped\n";

/* Verify via system table that it's gone */
$stmt = $pdo->query("SELECT COUNT(*) AS cnt FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'DDL_TEST'");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "exists: " . ($row['CNT'] == 0 ? "no" : "yes") . "\n";

$pdo = null;
echo "Done\n";
?>
--EXPECT--
created
altered
row: 1-Alice-30
dropped
exists: no
Done
