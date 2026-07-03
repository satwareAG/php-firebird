--TEST--
OOP: Statement multi-row fetch, re-execute, close lifecycle
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/common.inc';

$conn = oop_connect();
$tx = oop_begin_transaction($conn);

// Create test table with multiple rows
fbird_query($tx, 'CREATE TABLE OOP_STMT_TEST (ID INTEGER, VAL VARCHAR(20))');
fbird_commit($tx);

// Insert 3 rows
$tx2 = oop_begin_transaction($conn);
$stmt = $conn->prepare('INSERT INTO OOP_STMT_TEST (ID, VAL) VALUES (?, ?)', $tx2);
fbird_execute($stmt, 1, 'one');
fbird_execute($stmt, 2, 'two');
fbird_execute($stmt, 3, 'three');
$tx2->commit();

// Multi-row fetch
$tx3 = oop_begin_transaction($conn);
$stmt2 = $conn->prepare('SELECT ID, VAL FROM OOP_STMT_TEST ORDER BY ID', $tx3);
$rs = $stmt2->execute($tx3);

$rows = [];
while ($row = $rs->fetch()) {
    $rows[] = [(int)$row['ID'], $row['VAL']];
}
var_dump(count($rows));
var_dump($rows[0]);
var_dump($rows[1]);
var_dump($rows[2]);

// Exhausted
var_dump($rs->fetch());
$rs->close();

// Re-execute the same prepared statement
$rs2 = $stmt2->execute($tx3);
$row = $rs2->fetch();
var_dump((int)$row['ID']);
$rs2->close();

$tx3->commit();
oop_close($conn);
echo "done\n";
?>
--EXPECTF--
int(3)
array(2) {
  [0]=>
  int(1)
  [1]=>
  string(3) "one"
}
array(2) {
  [0]=>
  int(2)
  [1]=>
  string(3) "two"
}
array(2) {
  [0]=>
  int(3)
  [1]=>
  string(5) "three"
}
bool(false)
int(1)
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
