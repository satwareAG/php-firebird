--TEST--
feat: fbird_multi_query + more_results + next_result
--CREDITS--
v12.1.0 M2 (#372) - procedural parity RED test
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $test_base; $link = fbird_connect($test_base);
fbird_query($link, "RECREATE TABLE test_mq (id INT)");
$r = fbird_multi_query($link, "INSERT INTO test_mq VALUES (1); INSERT INTO test_mq VALUES (2);");
echo "OK multi_query: " . ($r ? "true" : "false") . "\n";
fbird_query($link, "DROP TABLE test_mq");
echo "=== DONE ===\n";
?>
--EXPECT--
OK multi_query: true
=== DONE ===

--CLEAN--
<?php require_once __DIR__ . '/../clean.inc'; ?>
