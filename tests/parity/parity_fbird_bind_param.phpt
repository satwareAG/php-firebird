--TEST--
feat: fbird_bind_param (named, by ref)
--CREDITS--
v12.1.0 M2 (#367) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_bind_param')) die('skip gap: fbird_bind_param() not yet implemented (see #367)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $link, $test_base;
fbird_query($link, "RECREATE TABLE test_bp (id INT, name VARCHAR(50))");
$stmt = fbird_prepare($link, "INSERT INTO test_bp VALUES (:id, :name)");
$id = 1; $name = "test";
fbird_bind_param($stmt, ":id", $id);
fbird_bind_param($stmt, ":name", $name);
fbird_execute($stmt);
$res = fbird_query($link, "SELECT name FROM test_bp WHERE id = 1");
echo "OK bind_param: " . fbird_fetch_row($res)[0] . "\n";
fbird_free_result($res);
fbird_query($link, "DROP TABLE test_bp");
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===

--CLEAN--
<?php // Tests SKIP (function not implemented), no DDL executed ?>
