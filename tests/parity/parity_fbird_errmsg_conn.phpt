--TEST--
feat: per-connection error context
--CREDITS--
v12.1.0 M2 (#369) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_errmsg_conn')) die('skip gap: fbird_errmsg_conn() not yet implemented (see #369)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global \$link;
// This test verifies per-connection error isolation
\$conn2 = fbird_connect(\$GLOBALS["test_base"] ?? "localhost:employee", "SYSDBA", "masterkey");
@fbird_query(\$link, "SELECT * FROM nonexistent_table_a");
\$err1 = fbird_errmsg(\$link);
@fbird_query(\$conn2, "SELECT * FROM nonexistent_table_b");
\$err2 = fbird_errmsg(\$conn2);
echo "OK per-conn error isolation: " . (\$err1 !== \$err2 ? "yes" : "no") . "\n";
fbird_close(\$conn2);
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===
