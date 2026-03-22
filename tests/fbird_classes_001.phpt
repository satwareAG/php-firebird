--TEST--
Firebird\Exception hierarchy: class registration and instanceof checks
--SKIPIF--
<?php
require_once __DIR__ . '/skipif.inc';
if (!extension_loaded('firebird')) die('skip firebird extension not loaded');
?>
--FILE--
<?php
// Verify Firebird\Exception hierarchy is registered
var_dump(class_exists('Firebird\\Exception'));
var_dump(class_exists('Firebird\\ConnectionException'));
var_dump(class_exists('Firebird\\QueryException'));
var_dump(class_exists('Firebird\\ServiceException'));

// Verify inheritance
$e = new Firebird\Exception('test', 42);
var_dump($e instanceof RuntimeException);
var_dump($e instanceof Firebird\Exception);
var_dump($e->getMessage());
var_dump($e->getCode());

$ce = new Firebird\ConnectionException('conn error', 1);
var_dump($ce instanceof Firebird\Exception);
var_dump($ce instanceof RuntimeException);

$qe = new Firebird\QueryException('query error', 2);
var_dump($qe instanceof Firebird\Exception);

$se = new Firebird\ServiceException('service error', 3);
var_dump($se instanceof Firebird\Exception);

echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
string(4) "test"
int(42)
bool(true)
bool(true)
bool(true)
bool(true)
done
