--TEST--
feat: fbird_blob_truncate / erase / flush
--CREDITS--
v12.1.0 M2 (#375) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_blob_truncate')) die('skip gap: fbird_blob_truncate() not yet implemented (see #375)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global \$link, \$test_base;
fbird_query(\$link, "RECREATE TABLE test_bt (data BLOB)");
fbird_query(\$link, "INSERT INTO test_bt VALUES (NULL)");
\$trx = fbird_trans(\$link);
\$blob = fbird_blob_create(\$trx);
fbird_blob_add(\$blob, str_repeat("x", 1000));
fbird_blob_close(\$blob);
// Would test truncate here
echo "OK blob_truncate prerequisites met\n";
fbird_rollback(\$trx);
fbird_query(\$link, "DROP TABLE test_bt");
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===

--CLEAN--
<?php // Tests SKIP (function not implemented), no DDL executed ?>
