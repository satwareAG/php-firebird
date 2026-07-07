--TEST--
feat: fbird_debug / dump_debug_info / trace
--CREDITS--
v12.1.0 M2 (#377) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_debug')) die('skip gap: fbird_debug() not yet implemented (see #377)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global \$link;
fbird_debug("test debug message");
echo "OK debug: called\n";
\$r = fbird_dump_debug_info(\$link);
echo "OK dump_debug_info: " . (\$r ? "true" : "false") . "\n";
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===
