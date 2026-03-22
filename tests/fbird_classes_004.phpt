--TEST--
Firebird\Statement and Firebird\ResultSet: prepare, execute, fetch
--SKIPIF--
<?php
require_once __DIR__ . '/skipif.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

var_dump(class_exists('Firebird\\Statement'));
var_dump(class_exists('Firebird\\ResultSet'));

$conn = new Firebird\Connection($test_base, $user, $password);
$tr = $conn->beginTransaction();

// prepare a simple SELECT
$stmt = $conn->prepare('SELECT 1 AS N FROM RDB$DATABASE', $tr);
var_dump($stmt instanceof Firebird\Statement);

$rs = $stmt->execute($tr);
var_dump($rs instanceof Firebird\ResultSet);

$row = $rs->fetch();
var_dump(is_array($row));
var_dump((int)$row['N']);

var_dump($rs->fetch()); // no more rows

$rs->close();
$tr->commit();
$conn->close();
echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
int(1)
bool(false)
done
