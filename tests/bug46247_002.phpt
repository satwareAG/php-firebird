--TEST--
Bug #46247 (fbird_set_event_handler() is allowing to pass callback without event)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

$db = fbird_connect($test_base);

fbird_set_event_handler('foo', 1);
fbird_set_event_handler($db, 'foo', 1);

?>
--EXPECTF--
Warning: fbird_set_event_handler(): Callback argument foo is not a callable function in %s on line %d

Warning: fbird_set_event_handler(): Callback argument foo is not a callable function in %s on line %d
