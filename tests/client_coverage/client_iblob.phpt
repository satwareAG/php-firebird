--TEST--
IBlob full lifecycle (FB3 baseline)
--CREDITS--
v12.1.0 M4 (#389) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
fbird_query($link, "RECREATE TABLE test_iblob (data BLOB)");
$trx = fbird_trans($link);
$blob = fbird_blob_create($trx);
fbird_blob_add($blob, "test blob data");
$blob_id = fbird_blob_close($blob);
$blob2 = fbird_blob_open($trx, $blob_id);
$data = "";
while ($chunk = fbird_blob_get($blob2, 100)) { $data .= $chunk; }
fbird_blob_info($trx, $blob_id);
fbird_blob_close($blob2);
fbird_rollback($trx);
fbird_query($link, "DROP TABLE test_iblob");
echo "done\n";
?>
--EXPECT--
done

--CLEAN--
<?php // Tables dropped in test body ?>
