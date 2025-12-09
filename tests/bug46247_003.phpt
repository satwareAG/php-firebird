--TEST--
Bug #46247 (fbird_set_event_handler() is allowing to pass callback without event)
--SKIPIF--
<?php
include("skipif.inc");
?>
--FILE--
<?php

require("firebird.inc");

$db = fbird_connect($test_base);

function test() { }

fbird_set_event_handler();

?>
--EXPECTF--
Fatal error: Uncaught ArgumentCountError: Wrong parameter count for fbird_set_event_handler() in %a
