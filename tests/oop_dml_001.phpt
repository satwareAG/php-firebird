--TEST--
OOP: DML (INSERT/SELECT) with parameterized query via procedural fbird_execute
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/common.inc';

$conn = oop_connect();
$tx = oop_begin_transaction($conn);

// Create test table using procedural API (OOP has no exec method)
fbird_query($tx, 'CREATE TABLE OOP_DML_TEST (ID INTEGER, NAME VARCHAR(50))');
fbird_commit($tx);

// Insert via OOP prepare + procedural fbird_execute (bind params)
$tx2 = oop_begin_transaction($conn);
$stmt = $conn->prepare('INSERT INTO OOP_DML_TEST (ID, NAME) VALUES (?, ?)', $tx2);
fbird_execute($stmt, 1, 'Alice');
fbird_execute($stmt, 2, 'Bob');
$tx2->commit();

// Verify via OOP query
$tx3 = oop_begin_transaction($conn);
$rs = oop_query($conn, $tx3, 'SELECT ID, NAME FROM OOP_DML_TEST ORDER BY ID');

$row1 = $rs->fetch();
var_dump((int) $row1['ID']);
var_dump($row1['NAME']);

$row2 = $rs->fetch();
var_dump((int) $row2['ID']);
var_dump($row2['NAME']);

var_dump($rs->fetch());
$rs->close();
$tx3->commit();

oop_close($conn);
echo "done\n";
?>
--EXPECTF--
int(1)
string(5) "Alice"
int(2)
string(3) "Bob"
bool(false)
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
