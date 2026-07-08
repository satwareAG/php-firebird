--TEST--
feat: fbird_send_long_data (stream BLOB to param)
--CREDITS--
v12.1.0 M2 (#380) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_send_long_data')) die('skip gap: fbird_send_long_data() not yet implemented (see #380)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $link, $test_base;
fbird_query($link, "RECREATE TABLE test_sld (data BLOB)");
$stmt = fbird_prepare($link, "INSERT INTO test_sld VALUES (?)");
$r = fbird_send_long_data($stmt, 1, str_repeat("x", 10000));
echo "OK send_long_data: " . ($r ? "true" : "false") . "\n";
fbird_free_query($stmt);
fbird_query($link, "DROP TABLE test_sld");
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===

--CLEAN--
<?php // Tests SKIP (function not implemented), no DDL executed ?>
