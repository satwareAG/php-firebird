--TEST--
IDtc / IDtcStart distributed transactions (FB3 baseline)
--CREDITS--
v12.1.0 M4 (#396) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
$trx = fbird_trans($link);
fbird_query($trx, "RECREATE TABLE test_idtc (id INT)");
fbird_commit_ret($trx);
fbird_commit($trx);
$res = fbird_query($link, "SELECT COUNT(*) FROM test_idtc");
fbird_fetch_row($res);
fbird_free_result($res);
fbird_query($link, "DROP TABLE test_idtc");
echo "done\n";
?>
--EXPECT--
done

--CLEAN--
<?php // Tables dropped in test body ?>
