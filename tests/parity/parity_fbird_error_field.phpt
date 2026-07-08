--TEST--
feat: structured diagnostic fields
--CREDITS--
v12.1.0 M2 (#371) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_error_field')) die('skip gap: fbird_error_field() not yet implemented (see #371)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $link;
@fbird_query($link, "SELECT * FROM nonexistent_table_df");
$sqlstate = fbird_error_field($link, 1); // FBIRD_DIAG_SQLSTATE
echo "OK error_field SQLSTATE: " . (strlen($sqlstate) > 0 ? "returned" : "empty") . "\n";
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===
