--TEST--
feat: fbird_error_list (array of all errors)
--CREDITS--
v12.1.0 M2 (#370) - procedural parity RED test
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $test_base; $link = fbird_connect($test_base);
@fbird_query($link, "SELECT * FROM nonexistent_table_el");
$errors = fbird_error_list($link);
echo "OK error_list: " . (is_array($errors) ? count($errors) . " errors" : "not array") . "\n";
echo "=== DONE ===\n";
?>
--EXPECTF--
OK error_list: %d errors
=== DONE ===
