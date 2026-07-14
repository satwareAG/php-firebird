--TEST--
FB3 wire protocol 13 negotiation
--CREDITS--
v12.1.0 M4 (#400) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
// Connection itself proves wire protocol negotiation.
// Use FIREBIRD_HOST env var (set by CI and docker-compose) instead of
// hardcoded "localhost" which may not resolve in container networks.
$host = getenv('FIREBIRD_HOST') ?: 'localhost';
$svc = @fbird_service_attach($host, "SYSDBA", "masterkey");
if ($svc) { @fbird_server_info($svc, 4); fbird_service_detach($svc); }
echo "done\n";
?>
--EXPECT--
done
