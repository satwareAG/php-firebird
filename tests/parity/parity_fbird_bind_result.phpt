--TEST--
feat: fbird_bind_result / define_by_name
--CREDITS--
v12.1.0 M2 (#368) - procedural parity RED test
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $test_base; $link = fbird_connect($test_base);
$res = fbird_query($link, "SELECT 42 AS VAL FROM rdb\$database");
fbird_bind_result($res, "VAL", $val);
fbird_fetch_row($res);
echo "OK bind_result: $val\n";
fbird_free_result($res);
echo "=== DONE ===\n";
?>
--EXPECTF--
%s
=== DONE ===
