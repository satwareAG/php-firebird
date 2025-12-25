--TEST--
Check for data types using old clients
--SKIPIF--
<?php
include("skipif.inc");
// This test requires a manually configured TEST_001 table with specific Firebird 4.0+ types
// (INT128, DECFLOAT, TIME_TZ, TIMESTAMP_TZ) and uses hardcoded database paths.
// Not suitable for CI - intended for local development testing only.
print "skip: requires manual TEST_001 table setup (development-only test)";
?>
--FILE--
<?php

require("firebird.inc");

(function(){
    // fbird_connect("127.0.0.1/3052:E:\\dbf50\\test\\TEST.FDB", "sysdba", "masterkey", "utf8");
    fbird_connect("127.0.0.1/3052:/opt/db/test.fdb", "sysdba", "masterkey", "utf8");
    fbird_query("DELETE FROM TEST_001");
    fbird_query("ALTER TABLE TEST_001 ALTER COLUMN ID RESTART WITH 1");
    fbird_query("INSERT INTO TEST_001 (ID) VALUES (DEFAULT)");
    // dump_table_rows("TEST_001", null, FBIRD_FETCH_BLOBS | FBIRD_UNIXTIME);
    dump_table_rows("TEST_001", null, FBIRD_FETCH_BLOBS);
})();

?>
--EXPECT--
array(17) {
  ["ID"]=>
  int(1)
  ["BLOB_0"]=>
  string(6) "BLOB_0"
  ["BLOB_1"]=>
  string(6) "BLOB_1"
  ["BOOL_1"]=>
  bool(true)
  ["DATE_1"]=>
  string(10) "2025-11-06"
  ["TIME_1"]=>
  string(8) "15:45:59"
  ["DECFLOAT_16"]=>
  string(17) "3.141592653589793"
  ["DECFLOAT_34"]=>
  string(35) "3.141592653589793238462643383279502"
  ["INT_NOT_NULL"]=>
  int(1)
  ["DOUBLE_PRECISION_1"]=>
  float(3.141592653589793)
  ["FLOAT_1"]=>
  float(3.1415927410125732)
  ["INT_1"]=>
  int(1)
  ["INT_128"]=>
  string(39) "170141183460469231731687303715884105727"
  ["VARCHAR_1"]=>
  string(9) "VARCHAR_1"
  ["SMALLINT_1"]=>
  int(1)
  ["TIME_TZ"]=>
  string(20) "15:45:59 Europe/Riga"
  ["TIMESTAMP_TZ"]=>
  string(31) "2025-11-06 15:45:59 Europe/Riga"
}
