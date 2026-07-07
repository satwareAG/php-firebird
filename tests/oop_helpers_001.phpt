--TEST--
OOP helpers: verify oop_connect/begin_transaction/query/close from common.inc
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/common.inc';

// oop_connect returns Firebird\Connection
$conn = oop_connect();
var_dump($conn instanceof \Firebird\Connection);
var_dump($conn->isConnected());

// oop_begin_transaction returns Firebird\Transaction
$tx = oop_begin_transaction($conn);
var_dump($tx instanceof \Firebird\Transaction);
var_dump($tx->isActive());

// oop_query returns Firebird\ResultSet with correct data
$rs = oop_query($conn, $tx, 'SELECT 1 AS N FROM RDB$DATABASE');
var_dump($rs instanceof \Firebird\ResultSet);

$row = $rs->fetch();
var_dump(is_array($row));
var_dump((int) $row['N']);

// No more rows
var_dump($rs->fetch());
$rs->close();

// Commit and close
$tx->commit();
var_dump($tx->isActive());

oop_close($conn);
var_dump($conn->isConnected());

echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
int(1)
bool(false)
bool(false)
bool(false)
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
