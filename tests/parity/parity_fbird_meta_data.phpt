--TEST--
feat: fbird_meta_data / list_tables / list_fields
--CREDITS--
v12.1.0 M2 (#373) - procedural parity RED test
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $test_base;
$link = fbird_connect($test_base);
$meta = fbird_meta_data($link, "RDB\$DATABASE");
echo "OK meta_data: " . (is_array($meta) ? "array" : "other") . "\n";
$tables = fbird_list_tables($link);
echo "OK list_tables: " . (is_array($tables) ? count($tables) . " tables" : "other") . "\n";
fbird_close($link);
echo "=== DONE ===\n";
?>
--EXPECT--
OK meta_data: array
OK list_tables: 1 tables
=== DONE ===
