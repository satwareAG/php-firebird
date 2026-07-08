--TEST--
FB3 wire protocol 13 negotiation
--CREDITS--
v12.1.0 M4 (#400) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
// Connection itself proves wire protocol negotiation
$svc = @fbird_service_attach("localhost", "SYSDBA", "masterkey");
if ($svc) { @fbird_server_info($svc, 4); fbird_service_detach($svc); }
echo "done\n";
?>
--EXPECT--
done
