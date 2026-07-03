--TEST--
OOP: Firebird\Service construct, isAttached, getServerVersion, detach
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

$svc = new \Firebird\Service($host ?: 'localhost', $user, $password);

var_dump($svc instanceof \Firebird\Service);
var_dump($svc->isAttached());

$version = $svc->getServerVersion();
var_dump(is_string($version));
var_dump(strlen($version) > 0);

$svc->detach();
var_dump($svc->isAttached());

echo "done\n";
?>
--EXPECTF--
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
