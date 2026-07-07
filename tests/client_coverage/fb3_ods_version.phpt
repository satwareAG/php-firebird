--TEST--
FB3 ODS 12.0 specific behaviors
--CREDITS--
v12.1.0 M4 (#401) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
fbird_connection_info($link);
$res = fbird_query($link, "SELECT rdb\$get_context('SYSTEM', 'ENGINE_VERSION') FROM rdb\$database");
fbird_fetch_row($res);
fbird_free_result($res);
echo "done\n";
?>
--EXPECT--
done
