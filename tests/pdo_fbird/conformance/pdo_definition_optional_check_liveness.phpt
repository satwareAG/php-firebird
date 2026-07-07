--TEST--
PDO Definition: optional check_liveness (ATTR_CONNECTION_STATUS)
--CREDITS--
v12.1.0 M1-8 (#335)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';
echo "=== Optional: check_liveness ===\n";
$pdo = pdo_fbird_connect();
$status = $pdo->getAttribute(PDO::ATTR_CONNECTION_STATUS);
echo "OK CONNECTION_STATUS: " . var_export($status, true) . "\n";
// A simple query proves the connection is live
$pdo->query("SELECT 1 FROM rdb\$database")->fetch();
echo "OK connection live (query succeeded)\n";
echo "=== DONE ===\n";
?>
--EXPECTF--
=== Optional: check_liveness ===
OK CONNECTION_STATUS: %s
OK connection live (query succeeded)
=== DONE ===
