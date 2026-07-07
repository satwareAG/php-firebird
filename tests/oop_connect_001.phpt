--TEST--
OOP: Connection lifecycle via oop_connect/oop_close helpers
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/common.inc';

$conn = oop_connect();
var_dump($conn instanceof \Firebird\Connection);
var_dump($conn->isConnected());

var_dump($conn->ping());

oop_close($conn);
var_dump($conn->isConnected());

echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(false)
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
