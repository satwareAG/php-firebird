--TEST--
pdo_fbird: fetch modes (ASSOC, NUM, BOTH, OBJ, COLUMN, KEY_PAIR)
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

$pdo->exec("RECREATE TABLE fetch_test (id INTEGER, name VARCHAR(20))");
$pdo->exec("INSERT INTO fetch_test VALUES (1, 'Alice')");
$pdo->exec("INSERT INTO fetch_test VALUES (2, 'Bob')");

/* FETCH_ASSOC */
$stmt = $pdo->query("SELECT id, name FROM fetch_test ORDER BY id");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "assoc: {$row['ID']}-{$row['NAME']}\n";

/* FETCH_NUM */
$stmt = $pdo->query("SELECT id, name FROM fetch_test ORDER BY id");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "num: {$row[0]}-{$row[1]}\n";

/* FETCH_BOTH */
$stmt = $pdo->query("SELECT id, name FROM fetch_test ORDER BY id");
$row = $stmt->fetch(PDO::FETCH_BOTH);
echo "both: {$row[0]}-{$row['NAME']}\n";

/* FETCH_OBJ */
$stmt = $pdo->query("SELECT id, name FROM fetch_test ORDER BY id");
$row = $stmt->fetch(PDO::FETCH_OBJ);
echo "obj: {$row->ID}-{$row->NAME}\n";

/* fetchColumn */
$stmt = $pdo->query("SELECT name FROM fetch_test ORDER BY id");
echo "col: " . $stmt->fetchColumn() . "\n";

/* fetchAll FETCH_KEY_PAIR */
$stmt = $pdo->query("SELECT id, name FROM fetch_test ORDER BY id");
$pairs = $stmt->fetchAll(PDO::FETCH_KEY_PAIR);
echo "pairs: " . implode(',', array_map(fn($k,$v) => "$k=$v", array_keys($pairs), $pairs)) . "\n";

$pdo->exec("DROP TABLE fetch_test");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
assoc: 1-Alice
num: 1-Alice
both: 1-Alice
obj: 1-Alice
col: Alice
pairs: 1=Alice,2=Bob
Done
