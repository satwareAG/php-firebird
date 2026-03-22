--TEST--
Firebird\Transaction: beginTransaction, commit, rollback, isActive
--SKIPIF--
<?php
require_once __DIR__ . '/skipif.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

var_dump(class_exists('Firebird\\Transaction'));

$conn = new Firebird\Connection($test_base, $user, $password);

$tr = $conn->beginTransaction();
var_dump($tr instanceof Firebird\Transaction);
var_dump($tr->isActive());

$tr->commit();
var_dump($tr->isActive());

$tr2 = $conn->beginTransaction();
var_dump($tr2->isActive());
$tr2->rollback();
var_dump($tr2->isActive());

$conn->close();
echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
bool(false)
done
