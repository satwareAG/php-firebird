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

fbird_set_event_handler(NULL, 'test', 1);

?>
--EXPECTF--
Fatal error: Uncaught TypeError: fbird_set_event_handler(): supplied argument is not a valid Firebird link resource in %a
