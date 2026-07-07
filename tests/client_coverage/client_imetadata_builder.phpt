--TEST--
IMetadataBuilder (FB3 baseline)
--CREDITS--
v12.1.0 M4 (#393) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
fbird_query($link, "RECREATE TABLE test_imb (id INT, val VARCHAR(100))");
fbird_query($link, "INSERT INTO test_imb VALUES (1, 'hello')");
$res = fbird_query($link, "SELECT * FROM test_imb");
$info = fbird_field_info($res, 0);
isset($info["name"]);
isset($info["type"]);
isset($info["length"]);
fbird_free_result($res);
fbird_query($link, "DROP TABLE test_imb");
echo "done\n";
?>
--EXPECT--
done

--CLEAN--
<?php // Tables dropped in test body ?>
