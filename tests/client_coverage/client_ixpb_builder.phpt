--TEST--
IXpbBuilder all kinds (FB3 baseline)
--CREDITS--
v12.1.0 M4 (#395) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
// DPB: connect with charset
$conn2 = fbird_connect($test_base, $user, $password, "UTF8");
if ($conn2) fbird_close($conn2);
// TPB: transaction with flags
$trx = fbird_trans($link);
if ($trx) fbird_commit($trx);
// SPB: service attach
// jane: use FIREBIRD_HOST env var - hardcoded "localhost" crashes in CI.
$host = getenv('FIREBIRD_HOST') ?: 'localhost';
$svc = @fbird_service_attach($host, "SYSDBA", "masterkey");
if ($svc) fbird_service_detach($svc);
echo "done\n";
?>
--EXPECT--
done
