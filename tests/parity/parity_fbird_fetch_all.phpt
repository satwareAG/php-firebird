--TEST--
feat: fbird_fetch_all
--CREDITS--
v12.1.0 M2 (#363) - procedural parity RED test
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $test_base; $link = fbird_connect($test_base);
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
OK fetch_all count: 2
=== DONE ===


--CLEAN--
<?php require_once __DIR__ . '/../clean.inc'; ?>
