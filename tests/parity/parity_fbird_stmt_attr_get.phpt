--TEST--
feat: fbird_stmt_attr_get / set
--CREDITS--
v12.1.0 M2 (#378) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_stmt_attr_get')) die('skip gap: fbird_stmt_attr_get() not yet implemented (see #378)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global \$link;
\$stmt = fbird_prepare(\$link, "SELECT 1 FROM rdb\$database");
\$val = fbird_stmt_attr_get(\$stmt, 1); // FBIRD_STMT_ATTR_PREFETCH
echo "OK stmt_attr_get: " . var_export(\$val, true) . "\n";
fbird_free_query(\$stmt);
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===
