--TEST--
OOP: SELECT query via oop_query helper with fetch
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/common.inc';

$conn = oop_connect();
$tx = oop_begin_transaction($conn);

// Simple SELECT
$rs = oop_query($conn, $tx, 'SELECT 1 AS N, \'hello\' AS S FROM RDB$DATABASE');
var_dump($rs instanceof \Firebird\ResultSet);

$row = $rs->fetch();
var_dump((int) $row['N']);
var_dump($row['S']);

// Exhausted
var_dump($rs->fetch());
$rs->close();

$tx->commit();
oop_close($conn);
echo "done\n";
?>
--EXPECTF--
bool(true)
int(1)
string(5) "hello"
bool(false)
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
