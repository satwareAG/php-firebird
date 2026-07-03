--TEST--
OOP: Transaction lifecycle via oop_begin_transaction helper
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/common.inc';

$conn = oop_connect();

$tx = oop_begin_transaction($conn);
var_dump($tx instanceof \Firebird\Transaction);
var_dump($tx->isActive());

$tx->commit();
var_dump($tx->isActive());

$tx2 = oop_begin_transaction($conn);
var_dump($tx2->isActive());
$tx2->rollback();
var_dump($tx2->isActive());

oop_close($conn);
echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(false)
bool(true)
bool(false)
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
