--TEST--
FB3 SQL features smoke (BOOLEAN, sub-routines, etc.)
--CREDITS--
v12.1.0 M4 (#399) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
fbird_query($link, "RECREATE TABLE test_fb3f (id INT, flag BOOLEAN, name VARCHAR(50))");
fbird_query($link, "INSERT INTO test_fb3f VALUES (1, TRUE, 'a')");
fbird_query($link, "INSERT INTO test_fb3f VALUES (2, FALSE, 'b')");
$res = fbird_query($link, "SELECT flag FROM test_fb3f WHERE id = 1");
fbird_fetch_row($res);
fbird_free_result($res);
// RETURNING
$trx = fbird_trans($link);
$res = fbird_query($trx, "INSERT INTO test_fb3f VALUES (3, TRUE, 'c') RETURNING id");
fbird_fetch_row($res);
fbird_free_result($res);
fbird_rollback($trx);
// GEN_ID
$res = fbird_query($link, "SELECT GEN_ID(GEN_LFDNR, 0) FROM rdb\$database");
fbird_fetch_row($res);
fbird_free_result($res);
// CREATE OR ALTER
// SAVEPOINT
$trx = fbird_trans($link);
fbird_savepoint($trx, "SP1");
fbird_query($trx, "INSERT INTO test_fb3f VALUES (4, FALSE, 'd')");
fbird_rollback_savepoint($trx, "SP1");
fbird_commit($trx);
// ROWS
$res = fbird_query($link, "SELECT id FROM test_fb3f ROWS 1");
fbird_fetch_row($res);
fbird_free_result($res);
// EXECUTE BLOCK
$res = fbird_query($link, "EXECUTE BLOCK RETURNS (x INT) AS BEGIN x = 42; SUSPEND; END");
fbird_fetch_row($res);
fbird_free_result($res);
// RECREATE
fbird_query($link, "RECREATE TABLE test_fb3f2 (id INT)");
fbird_query($link, "DROP TABLE test_fb3f2");
fbird_query($link, "DROP TABLE test_fb3f");
echo "done\n";
?>
--EXPECT--
done

--CLEAN--
<?php // Tables dropped in test body ?>
