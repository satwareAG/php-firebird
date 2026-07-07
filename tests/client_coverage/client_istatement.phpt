--TEST--
IStatement prepare flags + execute (FB3 baseline)
--CREDITS--
v12.1.0 M4 (#387) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
fbird_query($link, "RECREATE TABLE test_istmt (id INT, name VARCHAR(50))");
fbird_query($link, "INSERT INTO test_istmt VALUES (1, 'test')");
$stmt = fbird_prepare($link, "SELECT id, name FROM test_istmt WHERE id = ?");
$res = fbird_execute($stmt, 1);
fbird_fetch_assoc($res);
fbird_free_result($res);
$stmt2 = fbird_prepare($link, "INSERT INTO test_istmt VALUES (?, ?)");
fbird_execute($stmt2, 2, "test2");
fbird_free_query($stmt2);
$res3 = fbird_query($link, "SELECT id FROM test_istmt");
fbird_name_result($res3, "MY_CURSOR");
fbird_free_result($res3);
fbird_query($link, "DROP TABLE test_istmt");
echo "done\n";
?>
--EXPECT--
done
