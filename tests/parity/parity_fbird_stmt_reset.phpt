--TEST--
feat: fbird_stmt_reset (reset for re-execute)
--CREDITS--
v12.1.0 M2 (#381) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_stmt_reset')) die('skip gap: fbird_stmt_reset() not yet implemented (see #381)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global \$link, \$test_base;
fbird_query(\$link, "RECREATE TABLE test_sr (id INT)");
fbird_query(\$link, "INSERT INTO test_sr VALUES (1)");
\$stmt = fbird_prepare(\$link, "SELECT id FROM test_sr WHERE id = ?");
fbird_execute(\$stmt, 1);
fbird_fetch_row(\$stmt);
fbird_stmt_reset(\$stmt);
fbird_execute(\$stmt, 1);
\$row = fbird_fetch_row(\$stmt);
echo "OK stmt_reset: " . \$row[0] . "\n";
fbird_free_query(\$stmt);
fbird_query(\$link, "DROP TABLE test_sr");
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===
