--TEST--
OOP: Prepared statement reuse with different parameters
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/common.inc';

$conn = oop_connect();
$tx = oop_begin_transaction($conn);

// Create test table
fbird_query($tx, 'CREATE TABLE OOP_REUSE_TEST (ID INTEGER, NAME VARCHAR(30))');
fbird_commit($tx);

// Prepare once, execute multiple times with different params
$tx2 = oop_begin_transaction($conn);
$stmt = $conn->prepare('INSERT INTO OOP_REUSE_TEST (ID, NAME) VALUES (?, ?)', $tx2);

fbird_execute($stmt, 1, 'first');
fbird_execute($stmt, 2, 'second');
fbird_execute($stmt, 3, 'third');

$tx2->commit();

// Verify all 3 rows
$tx3 = oop_begin_transaction($conn);
$rs = oop_query($conn, $tx3, 'SELECT ID, NAME FROM OOP_REUSE_TEST ORDER BY ID');

$row1 = $rs->fetch();
var_dump((int)$row1['ID']);
var_dump($row1['NAME']);

$row2 = $rs->fetch();
var_dump((int)$row2['ID']);
var_dump($row2['NAME']);

$row3 = $rs->fetch();
var_dump((int)$row3['ID']);
var_dump($row3['NAME']);

var_dump($rs->fetch());
$rs->close();
$tx3->commit();

oop_close($conn);
echo "done\n";
?>
--EXPECTF--
int(1)
string(5) "first"
int(2)
string(6) "second"
int(3)
string(5) "third"
bool(false)
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
