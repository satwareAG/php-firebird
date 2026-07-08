--TEST--
feat: fbird_error_list (array of all errors)
--CREDITS--
v12.1.0 M2 (#370) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_error_list')) die('skip gap: fbird_error_list() not yet implemented (see #370)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $link;
@fbird_query($link, "SELECT * FROM nonexistent_table_el");
$errors = fbird_error_list($link);
echo "OK error_list: " . (is_array($errors) ? count($errors) . " errors" : "not array") . "\n";
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===
