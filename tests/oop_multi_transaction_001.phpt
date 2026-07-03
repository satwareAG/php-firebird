--TEST--
OOP: Two concurrent transactions on same connection
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/common.inc';

$conn = oop_connect();

// Create table first (before transactions)
fbird_query($conn, 'CREATE TABLE OOP_MULTI_TX (ID INTEGER)');
fbird_commit($conn);

// Start two independent transactions
$tx1 = oop_begin_transaction($conn);
$tx2 = oop_begin_transaction($conn);

var_dump($tx1 instanceof \Firebird\Transaction);
var_dump($tx2 instanceof \Firebird\Transaction);
var_dump($tx1->isActive());
var_dump($tx2->isActive());

// Insert in tx1
fbird_query($tx1, 'INSERT INTO OOP_MULTI_TX VALUES (1)');
// Insert in tx2
fbird_query($tx2, 'INSERT INTO OOP_MULTI_TX VALUES (2)');

// Commit tx1, rollback tx2
fbird_commit($tx1);
fbird_rollback($tx2);

var_dump($tx1->isActive());
var_dump($tx2->isActive());

// Verify: only tx1's insert survived
$tx3 = oop_begin_transaction($conn);
$rs = oop_query($conn, $tx3, 'SELECT COUNT(*) AS C FROM OOP_MULTI_TX');
$row = $rs->fetch();
var_dump((int) $row['C']);
$rs->close();
$tx3->commit();

oop_close($conn);
echo "done\n";
?>
--EXPECTF--
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)
int(1)
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
