--TEST--
IMessageMetadata all getters (FB3 baseline)
--CREDITS--
v12.1.0 M4 (#392) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
fbird_query($link, "RECREATE TABLE test_imeta (id INT, name VARCHAR(50))");
fbird_query($link, "INSERT INTO test_imeta VALUES (1, 'test')");
$res = fbird_query($link, "SELECT id, name FROM test_imeta");
fbird_num_fields($res);
fbird_field_info($res, 0);
fbird_field_info($res, 1);
$stmt = fbird_prepare($link, "SELECT * FROM test_imeta WHERE id = ?");
fbird_num_params($stmt);
fbird_free_query($stmt);
fbird_free_result($res);
fbird_query($link, "DROP TABLE test_imeta");
echo "done\n";
?>
--EXPECT--
done
