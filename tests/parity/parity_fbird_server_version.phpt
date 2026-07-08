--TEST--
feat: fbird_server_version at connection level
--CREDITS--
v12.1.0 M2 (#361) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_server_version')) die('skip gap: fbird_server_version() not yet implemented (see #361)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $link;
$ver = fbird_server_version($link);
echo "OK server_version: " . (strlen($ver) > 0 ? "returned" : "empty") . "\n";
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===
