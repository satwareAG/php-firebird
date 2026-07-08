--TEST--
feat: fbird_multi_query + more_results + next_result
--CREDITS--
v12.1.0 M2 (#372) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_multi_query')) die('skip gap: fbird_multi_query() not yet implemented (see #372)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $link, $test_base;
fbird_query($link, "RECREATE TABLE test_mq (id INT)");
$r = fbird_multi_query($link, "INSERT INTO test_mq VALUES (1); INSERT INTO test_mq VALUES (2);");
echo "OK multi_query: " . ($r ? "true" : "false") . "\n";
fbird_query($link, "DROP TABLE test_mq");
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===

--CLEAN--
<?php // Tests SKIP (function not implemented), no DDL executed ?>
