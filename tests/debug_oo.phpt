--TEST--
debug: close, affectedRows, beginTransaction
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';
require_once __DIR__ . '/../vendor/autoload.php';
$db = Firebird\Database::connect($test_base, $user, $password);

// beginTransaction
$tr = $db->beginTransaction();
var_dump($tr instanceof Firebird\TransactionManager);
var_dump($tr->isActive());

// affectedRows after insert
$db->queryWithTransaction($tr->getResource(), "INSERT INTO test1(i, c) VALUES(99, 'oo_test')");
$affected = $db->affectedRows();
var_dump($affected >= 0);
$tr->rollback();

// close
$r = $db->close();
var_dump($r);
var_dump($db->isConnected() === false);
echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
done
