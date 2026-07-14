--TEST--
feat: fbird_fetch_column
--CREDITS--
v12.1.0 M2 (#364) - procedural parity RED test
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $test_base; $link = fbird_connect($test_base);
$res = fbird_query($link, "SELECT 42 AS VAL FROM rdb\$database");
$val = fbird_fetch_column($res);
echo "OK fetch_column: $val\n";
fbird_free_result($res);
echo "=== DONE ===\n";
?>
--EXPECT--
OK fetch_column: 42
=== DONE ===
