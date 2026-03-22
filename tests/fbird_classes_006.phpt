--TEST--
Firebird\Service: class registration, attach, getServerVersion, detach
--SKIPIF--
<?php
require_once __DIR__ . '/skipif.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

var_dump(class_exists('Firebird\\Service'));

// Extract host from test_base (format: host/port:path or host:path or just path)
$host = 'localhost';
if (preg_match('/^([^:\/]+)[\/:]/', $test_base, $m)) {
    $host = $m[1];
}

$svc = new Firebird\Service($host, $user, $password);
var_dump($svc instanceof Firebird\Service);
var_dump($svc->isAttached());

$ver = $svc->getServerVersion();
var_dump(is_string($ver));
var_dump(strlen($ver) > 0);

$svc->detach();
var_dump($svc->isAttached());

echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
done
