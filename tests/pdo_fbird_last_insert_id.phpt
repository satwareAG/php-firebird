--TEST--
PDO Firebird: lastInsertId() with sequence name (#130)
--EXTENSIONS--
firebird
--SKIPIF--
<?php
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not loaded');
require_once __DIR__ . '/pdo_fbird.inc';
try { pdo_fbird_connect(); } catch (Throwable $e) { die('skip cannot connect: ' . $e->getMessage()); }
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

$pdo->exec("RECREATE SEQUENCE test_seq_pdo");
$pdo->exec("RECREATE TABLE test_lid (id INTEGER, name VARCHAR(50))");
$pdo->exec("CREATE OR ALTER TRIGGER test_lid_bi FOR test_lid ACTIVE BEFORE INSERT AS BEGIN NEW.id = GEN_ID(test_seq_pdo, 1); END");

/* Initial value */
$val = $pdo->lastInsertId("test_seq_pdo");
echo "initial: $val\n";

/* Increment via INSERT that fires the trigger */
$pdo->exec("INSERT INTO test_lid (name) VALUES ('Alice')");
$val2 = $pdo->lastInsertId("test_seq_pdo");
echo "after insert: $val2\n";

$pdo->exec("INSERT INTO test_lid (name) VALUES ('Bob')");
$val3 = $pdo->lastInsertId("test_seq_pdo");
echo "after insert 2: $val3\n";

/* Without name returns false */
$val4 = $pdo->lastInsertId();
var_dump($val4);

/* Cleanup */
$pdo->exec("DROP TABLE test_lid");
$pdo->exec("DROP SEQUENCE test_seq_pdo");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
initial: 0
after insert: 1
after insert 2: 2
bool(false)
Done
