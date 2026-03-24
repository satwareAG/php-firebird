--TEST--
fbird.enable_exceptions INI behavior
--SKIPIF--
<?php include("skipif.inc"); ?>
--INI--
fbird.enable_exceptions=1
--FILE--
<?php
require_once("firebird.inc");

echo "Default exception mode: " . fbird_get_exception_mode() . "\n";

$dsn = "nonexistent_host:nonexistent_db";
try {
    @fbird_connect($dsn, "SYSDBA", "masterkey");
    echo "Connection should have failed\n";
} catch (Firebird\Exception $e) {
    echo "Caught expected exception: " . $e->getMessage() . "\n";
}

fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_SILENT);
echo "New exception mode: " . fbird_get_exception_mode() . "\n";

ini_set('fbird.enable_exceptions', '0');
echo "INI after ini_set(0): " . ini_get('fbird.enable_exceptions') . "\n";
echo "RINIT should have refreshed mode: " . fbird_get_exception_mode() . "\n";

try {
    @fbird_connect($dsn, "SYSDBA", "masterkey");
    echo "No exception thrown (Silent mode)\n";
} catch (Firebird\Exception $e) {
    echo "Caught unexpected exception: " . $e->getMessage() . "\n";
}

?>
--EXPECTF--
Default exception mode: 1
Caught expected exception: %s
New exception mode: 0
INI after ini_set(0): 0
RINIT should have refreshed mode: 0
No exception thrown (Silent mode)
