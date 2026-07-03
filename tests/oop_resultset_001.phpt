--TEST--
OOP: ResultSet fetch modes and exhaustion
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/common.inc';

$conn = oop_connect();
$tx = oop_begin_transaction($conn);

// Create and populate test table
fbird_query($tx, 'CREATE TABLE OOP_RS_TEST (ID INTEGER, NAME VARCHAR(30))');
fbird_commit($tx);

$tx2 = oop_begin_transaction($conn);
fbird_query($tx2, "INSERT INTO OOP_RS_TEST VALUES (1, 'Alice')");
fbird_query($tx2, "INSERT INTO OOP_RS_TEST VALUES (2, 'Bob')");
fbird_commit($tx2);

// Fetch as associative array
$tx3 = oop_begin_transaction($conn);
$rs = oop_query($conn, $tx3, 'SELECT ID, NAME FROM OOP_RS_TEST ORDER BY ID');

$row = $rs->fetch();
var_dump(is_array($row));
var_dump((int)$row['ID']);
var_dump($row['NAME']);

$row2 = $rs->fetch();
var_dump((int)$row2['ID']);

// Exhausted
var_dump($rs->fetch());

// close() releases the cursor
$rs->close();
var_dump($rs->fetch());

$tx3->commit();
oop_close($conn);
echo "done\n";
?>
--EXPECTF--
bool(true)
int(1)
string(5) "Alice"
int(2)
bool(false)
bool(false)
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
