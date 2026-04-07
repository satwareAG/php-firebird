--TEST--
Firebird: use after fbird_free_query()
--SKIPIF--
<?php
include("skipif.inc");
if(!defined('FBIRD_VER') || (FBIRD_VER < 10)) print "Skip FBIRD_VER < 1.0.0";
?>
--FILE--
<?php

// Related to the "test execute procedure" part of tests/006.phpt. This
// ilustrates incorrect use of fbird_free_query(). If you follow the same logic
// as in 006.phpt, you would expect 5 batches with 2 rows printed with
// incremented I but it doesn't of course.

require("firebird.inc");
require("common.inc");
fbird_connect($test_base);

set_exception_handler("php_fbird_exception_handler");

test_use_after_fbird_free_query();

?>
--EXPECTF--
---- Batch 1 ----

Warning: fbird_fetch_assoc(): Argument #1 must be a valid (non-freed) Firebird query/result resource or Firebird\ResultSet in %s on line %d
Fatal error: Uncaught TypeError: fbird_free_result(): supplied resource is not a valid Firebird query resource
