--TEST--
feat: fbird_result_metadata (pre-execute)
--CREDITS--
v12.1.0 M2 (#379) - procedural parity RED test
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $test_base; $link = fbird_connect($test_base);
$stmt = fbird_prepare($link, "SELECT 1 AS VAL FROM rdb\$database");
$meta = fbird_result_metadata($stmt);
echo "OK result_metadata: " . (is_array($meta) ? count($meta) . " cols" : "other") . "\n";
fbird_free_query($stmt);
echo "=== DONE ===\n";
?>
--EXPECT--
OK result_metadata: 1 cols
=== DONE ===
