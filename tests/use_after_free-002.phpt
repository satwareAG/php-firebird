--TEST--
InterBase: use after ibase_free_query()
--SKIPIF--
<?php
include("skipif.inc");
if(!defined('FBIRD_VER') || (FBIRD_VER < 61)) print "Skip FBIRD_VER < 6.1";
?>
--FILE--
<?php

// Related to the "test execute procedure" part of tests/006.phpt. This
// ilustrates incorrect use of ibase_free_query(). If you follow the same logic
// as in 006.phpt, you would expect 5 batches with 2 rows printed with
// incremented I but it doesn't of course.

require("firebird.inc");
require("common.inc");
ibase_connect($test_base);

set_exception_handler("php_fbird_exception_handler");

test_use_after_fbird_free_query();

?>
--EXPECT--
---- Batch 1 ----
Fatal error: Uncaught TypeError: fbird_fetch_assoc(): supplied resource is not a valid Firebird/InterBase query resource
