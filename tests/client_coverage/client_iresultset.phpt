--TEST--
IResultSet fetchNext (FB3 baseline)
--CREDITS--
v12.1.0 M4 (#388) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
fbird_query($link, "RECREATE TABLE test_irs (id INT)");
fbird_query($link, "INSERT INTO test_irs VALUES (1)");
fbird_query($link, "INSERT INTO test_irs VALUES (2)");
fbird_query($link, "INSERT INTO test_irs VALUES (3)");
$res = fbird_query($link, "SELECT id FROM test_irs ORDER BY id");
fbird_fetch_row($res);
fbird_fetch_row($res);
fbird_fetch_row($res);
$past_end = fbird_fetch_row($res);
fbird_num_fields($res);
fbird_field_info($res, 0);
fbird_free_result($res);
fbird_query($link, "DROP TABLE test_irs");
echo "done\n";
?>
--EXPECT--
done

--CLEAN--
<?php // Tables dropped in test body ?>
