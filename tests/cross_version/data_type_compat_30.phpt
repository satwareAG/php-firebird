--TEST--
DataTypeCompatibility=3.0 mode (FB4+)
--CREDITS--
v12.1.0 M5 (#407) - cross-version compatibility test
--SKIPIF--
<?php
require_once __DIR__ . '/skipif.inc';
if (!is_fb_server_available("4.0")) die('skip FB 4.0 server not available');
?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
$conn = cross_version_connect("4.0");
$trx = fbird_trans($conn);
@fbird_query($trx, "DROP TABLE test_dtc30");
fbird_query($trx, "RECREATE TABLE test_dtc30 (id INT, dval DECFLOAT(34), ival INT128)");
fbird_query($trx, "INSERT INTO test_dtc30 VALUES (1, 3.14159265358979, 999999999999999999999)");
fbird_commit($trx);
$trx2 = fbird_trans($conn);
fbird_query($trx2, "SET BIND OF DECFLOAT TO DOUBLE PRECISION");
fbird_query($trx2, "SET BIND OF INT128 TO BIGINT");
$res = fbird_query($trx2, "SELECT dval, ival FROM test_dtc30 WHERE id = 1");
fbird_fetch_row($res);
fbird_free_result($res);
fbird_commit($trx2);
fbird_query($conn, "DROP TABLE test_dtc30");
fbird_close($conn);
echo "done\n";
?>
--EXPECT--
done
