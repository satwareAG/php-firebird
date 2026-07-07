--TEST--
DataTypeCompatibility=2.5 mode (FB4+)
--CREDITS--
v12.1.0 M5 (#406) - cross-version compatibility test
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
@fbird_query($trx, "DROP TABLE test_dtc25");
fbird_query($trx, "RECREATE TABLE test_dtc25 (id INT, flag BOOLEAN, dval DECFLOAT(16), ival INT128)");
fbird_query($trx, "INSERT INTO test_dtc25 VALUES (1, TRUE, 3.14, 999999999999999999)");
fbird_commit($trx);
$res = fbird_query($conn, "SELECT flag, dval, ival FROM test_dtc25 WHERE id = 1");
fbird_fetch_row($res);
fbird_free_result($res);
$trx2 = fbird_trans($conn);
fbird_query($trx2, "SET BIND OF DECFLOAT TO DOUBLE PRECISION");
fbird_query($trx2, "SET BIND OF INT128 TO BIGINT");
$res2 = fbird_query($trx2, "SELECT flag, dval, ival FROM test_dtc25 WHERE id = 1");
fbird_fetch_row($res2);
fbird_free_result($res2);
fbird_commit($trx2);
fbird_query($conn, "DROP TABLE test_dtc25");
fbird_close($conn);
echo "done\n";
?>
--EXPECT--
done

--CLEAN--
<?php // Tests SKIP on FB3 (needs FB4+), no DDL executed ?>
