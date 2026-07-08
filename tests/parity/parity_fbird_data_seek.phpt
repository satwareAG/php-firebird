--TEST--
feat: fbird_data_seek (procedural scrollable)
--CREDITS--
v12.1.0 M2 (#362) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_data_seek')) die('skip gap: fbird_data_seek() not yet implemented (see #362)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $link, $test_base;
fbird_query($link, "RECREATE TABLE test_seek (id INT)");
fbird_query($link, "INSERT INTO test_seek VALUES (1)");
fbird_query($link, "INSERT INTO test_seek VALUES (2)");
fbird_query($link, "INSERT INTO test_seek VALUES (3)");
$res = fbird_query($link, "SELECT id FROM test_seek ORDER BY id");
fbird_data_seek($res, 2);
$row = fbird_fetch_row($res);
echo "OK data_seek to row 2: " . $row[0] . "\n";
fbird_free_result($res);
fbird_query($link, "DROP TABLE test_seek");
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===

--CLEAN--
<?php // Tests SKIP (function not implemented), no DDL executed ?>
