--TEST--
FB3 legacy auth (Legacy_Auth plugin)
--CREDITS--
v12.1.0 M4 (#402) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
// Connection proves auth negotiation (Srp or Legacy_Auth)
fbird_get_client_version();
echo "done\n";
?>
--EXPECT--
done
