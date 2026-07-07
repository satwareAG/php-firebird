--TEST--
ITransaction all commit/rollback variants (FB3 baseline)
--CREDITS--
v12.1.0 M4 (#386) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
fbird_query($link, "RECREATE TABLE test_itrx (id INT)");
$trx = fbird_trans($link);
fbird_query($trx, "INSERT INTO test_itrx VALUES (1)");
fbird_commit($trx);
$trx = fbird_trans($link);
fbird_query($trx, "INSERT INTO test_itrx VALUES (2)");
fbird_commit_ret($trx);
fbird_commit($trx);
$trx = fbird_trans($link);
fbird_query($trx, "INSERT INTO test_itrx VALUES (3)");
fbird_rollback($trx);
$trx = fbird_trans($link);
fbird_query($trx, "INSERT INTO test_itrx VALUES (4)");
fbird_rollback_ret($trx);
fbird_rollback($trx);
$trx = fbird_trans($link);
fbird_savepoint($trx, "SP1");
fbird_query($trx, "INSERT INTO test_itrx VALUES (5)");
fbird_rollback_savepoint($trx, "SP1");
fbird_commit($trx);
fbird_query($link, "DROP TABLE test_itrx");
echo "done\n";
?>
--EXPECT--
done

--CLEAN--
<?php // Tables dropped in test body ?>
