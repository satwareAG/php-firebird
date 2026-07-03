--TEST--
OOP: Transaction retain (commit_ret / rollback_ret) via procedural bridge
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/common.inc';

$conn = oop_connect();

// commit_ret: commit but keep transaction open
$tx = oop_begin_transaction($conn);
fbird_query($tx, 'CREATE TABLE OOP_RETAIN_TEST (ID INTEGER)');
fbird_commit_ret($tx);
var_dump($tx->isActive());

// Insert within the retained transaction
fbird_query($tx, 'INSERT INTO OOP_RETAIN_TEST VALUES (1)');
fbird_commit_ret($tx);
var_dump($tx->isActive());

// rollback_ret: rollback but keep transaction open
fbird_query($tx, 'INSERT INTO OOP_RETAIN_TEST VALUES (2)');
fbird_rollback_ret($tx);
var_dump($tx->isActive());

// Verify only the first insert survived
fbird_query($tx, 'INSERT INTO OOP_RETAIN_TEST VALUES (3)');
fbird_commit($tx);
var_dump($tx->isActive());

// Verify data
$tx2 = oop_begin_transaction($conn);
$rs = oop_query($conn, $tx2, 'SELECT COUNT(*) AS C FROM OOP_RETAIN_TEST');
$row = $rs->fetch();
var_dump((int) $row['C']);
$rs->close();
$tx2->commit();

oop_close($conn);
echo "done\n";
?>
--EXPECTF--
bool(true)
bool(true)
bool(true)
bool(false)
int(2)
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
