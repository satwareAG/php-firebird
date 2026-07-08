--TEST--
feat: fbird_fetch_column
--CREDITS--
v12.1.0 M2 (#364) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_fetch_column')) die('skip gap: fbird_fetch_column() not yet implemented (see #364)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $link;
$res = fbird_query($link, "SELECT 42 AS VAL FROM rdb\$database");
$val = fbird_fetch_column($res);
echo "OK fetch_column: $val\n";
fbird_free_result($res);
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===
