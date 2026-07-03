--TEST--
fbird_drop_db(): Make sure passing an integer to the function throws an error.
--SKIPIF--
<?php
include("skipif.inc");
?>
--FILE--
<?php

require("firebird.inc");

$db_dir = getenv("FIREBIRD_DB_DIR") ?: sys_get_temp_dir();
$file = $db_dir . "/php_fbird_drop_" . bin2hex(random_bytes(4));
if(!empty($host))$file = "$host:$file";

$db = fbird_query(FBIRD_CREATE,
		sprintf("CREATE SCHEMA '%s' USER '%s' PASSWORD '%s' DEFAULT CHARACTER SET %s",$file,
		$user, $password, ($charset = ini_get('fbird.default_charset')) ? $charset : 'NONE'));

var_dump($db);
var_dump(fbird_drop_db($db));
var_dump(fbird_drop_db(1));

?>
--EXPECTF--
Deprecated: fbird_query(): Passing FBIRD_CREATE to fbird_query() is deprecated, use fbird_create_database() instead in %s on line %d
resource(%d) of type (Firebird link)
bool(true)

Fatal error: Uncaught TypeError: fbird_drop_db(): Argument #1 ($link_identifier) must be of type resource, int given in %a

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
