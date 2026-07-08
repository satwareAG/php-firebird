--TEST--
feat: fbird_result_metadata (pre-execute)
--CREDITS--
v12.1.0 M2 (#379) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_result_metadata')) die('skip gap: fbird_result_metadata() not yet implemented (see #379)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $link;
$stmt = fbird_prepare($link, "SELECT 1 AS VAL FROM rdb\$database");
$meta = fbird_result_metadata($stmt);
echo "OK result_metadata: " . (is_array($meta) ? count($meta) . " cols" : "other") . "\n";
fbird_free_query($stmt);
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===
