--TEST--
feat: fbird_debug / dump_debug_info / trace
--CREDITS--
v12.1.0 M2 (#377) - procedural parity RED test
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $test_base; $link = fbird_connect($test_base);
fbird_debug("test debug message");
echo "OK debug: called\n";
$r = fbird_dump_debug_info($link);
echo "OK dump_debug_info: " . ($r ? "true" : "false") . "\n";
echo "=== DONE ===\n";
?>
--EXPECTF--
Notice: %s
OK debug: called
OK dump_debug_info: true
=== DONE ===
