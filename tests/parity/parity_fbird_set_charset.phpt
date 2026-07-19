--TEST--
feat: fbird_set_charset / character_set_name / get_charset
--CREDITS--
v12.1.0 M2 (#366) - procedural parity RED test
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $test_base; $link = fbird_connect($test_base);
fbird_set_charset($link, "UTF8");
$cs = fbird_character_set_name($link);
echo "OK character_set_name: $cs\n";
echo "=== DONE ===\n";
?>
--EXPECT--
OK character_set_name: UTF8
=== DONE ===
