--TEST--
Bug #46543 (ibase_trans() memory leaks when using wrong parameters)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("interbase.inc");

@ibase_close();

try {
    ibase_trans(1);
} catch (TypeError $e) {
    echo $e->getMessage() . "\n";
}

try {
    ibase_trans();
} catch (TypeError $e) {
    echo $e->getMessage() . "\n";
}

try {
    ibase_trans('foo');
} catch (TypeError $e) {
    echo $e->getMessage() . "\n";
}
try {
    ibase_trans(fopen(__FILE__, 'r'));
} catch (TypeError $e) {
    echo $e->getMessage() . "\n";
}


$x = ibase_connect($test_base);
ibase_trans(1, 2, $x, $x, 3);

?>
--EXPECTF--
ibase_trans(): supplied resource is not a valid Firebird/InterBase link resource
ibase_trans(): supplied resource is not a valid Firebird/InterBase link resource
ibase_trans(): supplied resource is not a valid Firebird/InterBase link resource
ibase_trans(): supplied resource is not a valid Firebird/InterBase link resource
