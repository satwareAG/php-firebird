--TEST--
IRequest BLR (FB3 baseline)
--CREDITS--
v12.1.0 M4 (#397) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
// BLR is used internally by the driver
$res = fbird_query($link, "SELECT 1 FROM rdb\$database");
fbird_fetch_row($res);
fbird_free_result($res);
echo "done\n";
?>
--EXPECT--
done
