--TEST--
IUtil kitchen sink (FB3 baseline)
--CREDITS--
v12.1.0 M4 (#394) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
fbird_get_client_version();
$svc = @fbird_service_attach("localhost", "SYSDBA", "masterkey");
if ($svc) { @fbird_server_info($svc, 4); fbird_service_detach($svc); }
@fbird_gen_id("GEN_LFDNR", 0);
@fbird_query($link, "SELECT * FROM nonexistent_iutil");
fbird_errmsg();
echo "done\n";
?>
--EXPECT--
done
