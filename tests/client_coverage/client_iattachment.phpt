--TEST--
IAttachment full lifecycle (FB3 baseline)
--CREDITS--
v12.1.0 M4 (#385) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
// attach, getInfo, startTransaction, prepare, execute, createBlob, cancel, detach
$info = fbird_connection_info($link);
$trx = fbird_trans($link);
$stmt = fbird_prepare($trx, "SELECT 1 FROM rdb\$database");
fbird_free_query($stmt);
$res = fbird_query($link, "SELECT 1 FROM rdb\$database");
fbird_free_result($res);
$blob = fbird_blob_create($trx);
fbird_blob_cancel($blob);
fbird_commit($trx);
echo "done\n";
?>
--EXPECT--
done
