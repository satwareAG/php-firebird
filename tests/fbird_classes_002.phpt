--TEST--
Firebird\Connection: class registration, connect, ping, isConnected, close
--SKIPIF--
<?php
require_once __DIR__ . '/skipif.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

var_dump(class_exists('Firebird\\Connection'));

$conn = new Firebird\Connection($test_base, $user, $password);
var_dump($conn instanceof Firebird\Connection);
var_dump($conn->isConnected());
var_dump($conn->ping());

$conn->close();
var_dump($conn->isConnected());

echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
done
