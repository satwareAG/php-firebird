--TEST--
ibase_drop_db(): Basic test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("interbase.inc");

unlink($file = tempnam('/tmp',"php_ibase_test"));


$db = ibase_query(IBASE_CREATE,
		sprintf("CREATE SCHEMA '%s' USER '%s' PASSWORD '%s' DEFAULT CHARACTER SET %s",$file,
		$user, $password, ($charset = ini_get('ibase.default_charset')) ? $charset : 'NONE'));

var_dump($db);
var_dump(ibase_drop_db($db));
try {
    var_dump(ibase_drop_db(1));
} catch (TypeError $e) {
    echo $e->getMessage() . "\n";
}

try {
    var_dump(ibase_drop_db(null));
} catch (TypeError $e) {
    echo $e->getMessage() . "\n";
}

?>
--EXPECTF--
resource(10) of type (Firebird/InterBase link)
bool(true)
ibase_drop_db(): Argument #1 ($link_identifier) must be of type resource, int given
ibase_drop_db(): Argument #1 ($link_identifier) must be of type resource, null given

Warning: ibase_drop_db(): lock time-out on wait transaction object /tmp/%s is in use  in %s/interbase.inc on line %d