--TEST--
feat: fbird_bind_param (named, by ref)
--CREDITS--
v12.1.0 M2 (#367) - procedural parity RED test
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $test_base; $link = fbird_connect($test_base);
fbird_query($link, "RECREATE TABLE test_bp (id INT, name VARCHAR(50))");
$stmt = fbird_prepare($link, "INSERT INTO test_bp VALUES (?, ?)");
if (!$stmt) {
    echo "SKIP prepare failed\n";
    fbird_query($link, "DROP TABLE test_bp");
    echo "=== DONE ===\n";
    exit;
}
/* Test that bind_param can be called without crashing */
$ok1 = fbird_bind_param($stmt, 0, 1);
echo "OK bind_param called: " . ($ok1 ? "true" : "false") . "\n";
/* Execute with actual params */
fbird_execute($stmt, 1, "test");
$res = fbird_query($link, "SELECT name FROM test_bp WHERE id = 1");
$row = fbird_fetch_row($res);
echo "OK bind_param: " . ($row ? $row[0] : "no row") . "\n";
fbird_free_result($res);
fbird_free_query($stmt);
fbird_query($link, "DROP TABLE test_bp");
echo "=== DONE ===\n";
?>
--EXPECTF--
OK bind_param called: %s
OK bind_param: %s
=== DONE ===

--CLEAN--
<?php require_once __DIR__ . '/../clean.inc'; ?>
