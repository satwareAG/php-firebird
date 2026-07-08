--TEST--
FB3 SQL dialect 1 vs 3
--CREDITS--
v12.1.0 M4 (#403) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
$conn3 = fbird_connect($test_base, $user, $password, "", 0, 3);
$res = fbird_query($conn3, "SELECT CAST('2026-07-07' AS DATE) FROM rdb\$database");
fbird_fetch_row($res);
fbird_free_result($res);
fbird_close($conn3);
$conn1 = @fbird_connect($test_base, $user, $password, "", 0, 1);
if ($conn1) {
    $res = fbird_query($conn1, "SELECT CAST('2026-07-07' AS DATE) FROM rdb\$database");
    fbird_fetch_row($res);
    fbird_free_result($res);
    fbird_close($conn1);
}
echo "done\n";
?>
--EXPECT--
done
