--TEST--
feat: fbird_server_version at connection level
--CREDITS--
v12.1.0 M2 (#361) - procedural parity RED test
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $test_base;
$link = fbird_connect($test_base);
$ver = fbird_server_version($link);
echo "OK server_version: " . (strlen($ver) > 0 ? "returned" : "empty") . "\n";
if (strlen($ver) > 0) {
    echo "OK version contains Firebird: " . (strpos($ver, "Firebird") !== false || strpos($ver, "LI-V") !== false ? "yes" : "no") . "\n";
}
fbird_close($link);
echo "=== DONE ===\n";
?>
--EXPECT--
OK server_version: returned
OK version contains Firebird: yes
=== DONE ===
