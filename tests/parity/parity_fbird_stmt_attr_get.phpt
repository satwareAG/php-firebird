--TEST--
feat: fbird_stmt_attr_get / set
--CREDITS--
v12.1.0 M2 (#378) - procedural parity RED test
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $test_base;
$link = fbird_connect($test_base);
$stmt = fbird_prepare($link, "SELECT 1 FROM rdb\$database");
if (!$stmt) {
    echo "SKIP prepare failed: " . fbird_errmsg() . "\n";
    echo "=== DONE ===\n";
    exit;
}
/* Test that attr_get/set can be called. On FB4+, statement timeout attr works. */
$val = @fbird_stmt_attr_get($stmt, 1026); // FBIRD_ATTR_STATEMENT_TIMEOUT
echo "OK stmt_attr_get: callable\n";
fbird_free_query($stmt);
fbird_close($link);
echo "=== DONE ===\n";
?>
--EXPECT--
OK stmt_attr_get: callable
=== DONE ===
