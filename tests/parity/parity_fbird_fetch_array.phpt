--TEST--
feat: fbird_fetch_array (BOTH mode) - regression vs interbase
--CREDITS--
v12.1.0 M2 (#359) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_fetch_array')) die('skip gap: fbird_fetch_array() not yet implemented (see #359)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global \$link, \$test_base;
\$res = fbird_query(\$link, "SELECT 1 AS VAL FROM rdb\$database");
\$row = fbird_fetch_array(\$res);
echo "OK fetch_array has numeric: " . (isset(\$row[0]) ? "yes" : "no") . "\n";
echo "OK fetch_array has assoc: " . (isset(\$row["VAL"]) ? "yes" : "no") . "\n";
fbird_free_result(\$res);
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===
