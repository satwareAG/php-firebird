--TEST--
feat: fbird_fetch_all
--CREDITS--
v12.1.0 M2 (#363) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_fetch_all')) die('skip gap: fbird_fetch_all() not yet implemented (see #363)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $link, $test_base;
fbird_query($link, "RECREATE TABLE test_fa (id INT)");
fbird_query($link, "INSERT INTO test_fa VALUES (1)");
fbird_query($link, "INSERT INTO test_fa VALUES (2)");
$res = fbird_query($link, "SELECT id FROM test_fa ORDER BY id");
$rows = fbird_fetch_all($res);
echo "OK fetch_all count: " . count($rows) . "\n";
fbird_free_result($res);
fbird_query($link, "DROP TABLE test_fa");
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===

--CLEAN--
<?php // Tests SKIP (function not implemented), no DDL executed ?>
