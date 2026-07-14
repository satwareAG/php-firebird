--TEST--
feat: fbird_fetch_array (BOTH mode) - regression vs interbase
--CREDITS--
v12.1.0 M2 (#359) - procedural parity RED test
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $test_base;
$link = fbird_connect($test_base);
$res = fbird_query($link, "SELECT 1 AS VAL FROM rdb\$database");
$row = fbird_fetch_array($res);
echo "OK fetch_array has numeric: " . (isset($row[0]) ? "yes" : "no") . "\n";
echo "OK fetch_array has assoc: " . (isset($row["VAL"]) ? "yes" : "no") . "\n";
echo "OK numeric and assoc match: " . ($row[0] === $row["VAL"] ? "yes" : "no") . "\n";
fbird_free_result($res);
fbird_close($link);
echo "=== DONE ===\n";
?>
--EXPECT--
OK fetch_array has numeric: yes
OK fetch_array has assoc: yes
OK numeric and assoc match: yes
=== DONE ===
