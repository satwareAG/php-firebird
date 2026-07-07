--TEST--
feat: fbird_ping (procedural)
--CREDITS--
v12.1.0 M2 (#360) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_ping')) die('skip gap: fbird_ping() not yet implemented (see #360)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global \$link;
\$r = fbird_ping(\$link);
echo "OK ping: " . (\$r ? "true" : "false") . "\n";
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===
