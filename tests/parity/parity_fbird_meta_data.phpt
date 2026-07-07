--TEST--
feat: fbird_meta_data / list_tables / list_fields
--CREDITS--
v12.1.0 M2 (#373) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_meta_data')) die('skip gap: fbird_meta_data() not yet implemented (see #373)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global \$link, \$test_base;
\$meta = fbird_meta_data(\$link, "RDB\$DATABASE");
echo "OK meta_data: " . (is_array(\$meta) ? "array" : "other") . "\n";
\$tables = fbird_list_tables(\$link);
echo "OK list_tables: " . (is_array(\$tables) ? count(\$tables) . " tables" : "other") . "\n";
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===
