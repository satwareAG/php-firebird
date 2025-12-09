--TEST--
Bug #46247 (ibase_set_event_handler() is allowing to pass callback without event)
--SKIPIF--
<?php
include("skipif.inc");
include("skipif-php80-or-older.inc");
?>
--FILE--
<?php

require("firebird.inc");

$db = ibase_connect($test_base);

function test() { }

ibase_set_event_handler();

?>
--EXPECTF--
Fatal error: Uncaught ArgumentCountError: Wrong parameter count for ibase_set_event_handler() in %a
