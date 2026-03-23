--TEST--
fbird_drop_db(): Basic test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("config.inc");

unlink($file = tempnam(sys_get_temp_dir(),"php_fbird_test"));
if(!empty($host))$file = "$host:$file";

$db = fbird_query(FBIRD_CREATE,
		sprintf("CREATE SCHEMA '%s' USER '%s' PASSWORD '%s' DEFAULT CHARACTER SET %s",$file,
		$user, $password, ($charset = ini_get('fbird.default_charset')) ? $charset : 'NONE'));

var_dump($db);
var_dump(fbird_drop_db($db));

?>
--EXPECTF--
Deprecated: fbird_query(): Passing FBIRD_CREATE to fbird_query() is deprecated, use fbird_create_database() instead in %s on line %d
resource(%d) of type (Firebird link)
bool(true)
