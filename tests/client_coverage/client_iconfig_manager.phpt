--TEST--
IConfigManager / IFirebirdConf (FB3 baseline)
--CREDITS--
v12.1.0 M4 (#398) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
fbird_connection_info($link);
fbird_get_client_version();
echo "done\n";
?>
--EXPECT--
done
