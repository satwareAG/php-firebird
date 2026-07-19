--TEST--
feat: fbird_ping (procedural)
--CREDITS--
v12.1.0 M2 (#360) - procedural parity RED test
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $test_base;
$link = fbird_connect($test_base);
$r = fbird_ping($link);
echo "OK ping: " . ($r ? "true" : "false") . "\n";
fbird_close($link);
echo "=== DONE ===\n";
?>
--EXPECT--
OK ping: true
=== DONE ===
