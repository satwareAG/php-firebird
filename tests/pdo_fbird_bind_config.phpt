--TEST--
PDO Firebird: SET BIND attribute (FB 4+ only)
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not available');
require_once __DIR__ . '/pdo_fbird.inc';
try { $pdo = pdo_fbird_connect(); } catch (Throwable $e) { die('skip cannot connect: ' . $e->getMessage()); }
/* SET BIND requires Firebird 4.0+ */
$v = $pdo->getAttribute(PDO::ATTR_SERVER_VERSION);
if (floatval($v) < 4.0) die('skip requires Firebird 4.0+');
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

/* Test SET BIND attribute — coerce DECFLOAT to VARCHAR */
$result = $pdo->setAttribute(PDO::FBIRD_ATTR_SET_BIND, "DECFLOAT TO VARCHAR");
echo "set bind: " . ($result ? "ok" : "fail") . "\n";

/* Verify get returns empty (write-only) */
$val = $pdo->getAttribute(PDO::FBIRD_ATTR_SET_BIND);
echo "get bind: '" . $val . "'\n";

$pdo = null;
echo "Done\n";
?>
--EXPECT--
set bind: ok
get bind: ''
Done
