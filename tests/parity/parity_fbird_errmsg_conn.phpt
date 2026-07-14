--TEST--
feat: per-connection error context
--CREDITS--
v12.1.0 M2 (#369) - procedural parity RED test
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $test_base;
$link = fbird_connect($test_base);
$conn2 = fbird_connect($test_base);
/* Test that fbird_errmsg accepts optional link argument */
@fbird_query($link, "SELECT * FROM nonexistent_table_a");
$err1 = fbird_errmsg($link);
$err2 = fbird_errmsg($conn2);
$err3 = fbird_errmsg(); /* BC: no args */
echo "OK errmsg with link: " . (is_string($err1) ? "string" : (is_bool($err1) ? "false" : "other")) . "\n";
echo "OK errmsg BC no-arg: " . (is_string($err3) || is_bool($err3) ? "ok" : "fail") . "\n";
fbird_close($conn2);
echo "=== DONE ===\n";
?>
--EXPECT--
OK errmsg with link: string
OK errmsg BC no-arg: ok
=== DONE ===
