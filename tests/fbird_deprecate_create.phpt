--TEST--
fbird_query(FBIRD_CREATE) emits E_DEPRECATED
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
?>
--FILE--
<?php
/* Set SILENT so errors don't throw */
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_SILENT);

$caught = false;
set_error_handler(function($errno, $errstr) use (&$caught) {
    if ($errno === E_DEPRECATED) {
        echo "DEPRECATED: $errstr\n";
        $caught = true;
    }
    return true;
});

/* The deprecation fires before the SQL runs, even if create fails */
$result = fbird_query(FBIRD_CREATE, "CREATE DATABASE 'nonexistent_path_xyz'");

if ($caught) {
    echo "Done\n";
} else {
    echo "FAIL: no deprecation notice\n";
}
?>
--EXPECTF--
DEPRECATED: fbird_query(): Passing FBIRD_CREATE to fbird_query() is deprecated, use fbird_create_database() instead
Done

--CLEAN--
<?php
require_once 'config.inc';
// Database creation tests - DB dropped in --FILE-- section.
// This --CLEAN-- is a safety net for crash recovery.
?>
