--TEST--
Check comprehensive data types including Firebird 4.0+ types
--SKIPIF--
<?php
include("skipif.inc");
skip_if_fb_lt(4);
skip_if_fbclient_lt(4);
?>
--FILE--
<?php

require("firebird.inc");

fbird_connect($test_base);

// Create the TEST_001 table with all data types
fbird_query(file_get_contents(__DIR__."/001-DATATYPE.sql"));
fbird_commit();

// Insert a row with explicit BLOB values (BLOB doesn't support DEFAULT in Firebird)
fbird_query("INSERT INTO TEST_001 (BLOB_0, BLOB_1) VALUES ('BLOB_0', 'BLOB_1')");

// Fetch and display results
dump_table_rows("TEST_001", null, FBIRD_FETCH_BLOBS);

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
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
