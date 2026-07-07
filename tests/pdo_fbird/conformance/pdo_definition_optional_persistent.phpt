--TEST--
PDO Definition: optional persistent connections
--CREDITS--
v12.1.0 M1-9 (#336)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';
echo "=== Optional: persistent ===\n";

// Non-persistent (default)
$pdo1 = pdo_fbird_connect();
$is_pers = $pdo1->getAttribute(PDO::ATTR_PERSISTENT);
echo "OK default persistent: " . var_export($is_pers, true) . "\n";

// Enable persistent - setAttribute on an existing connection doesn't make it
// persistent (that's decided at construction), but it should not throw.
$r = $pdo1->setAttribute(PDO::ATTR_PERSISTENT, true);
echo "OK set persistent return: " . var_export($r, true) . "\n";

// Close and reopen
unset($pdo1);
echo "OK unset (no crash)\n";

echo "=== DONE ===\n";
?>
--EXPECTF--
=== Optional: persistent ===
OK default persistent: false
OK set persistent return: %s
OK unset (no crash)
=== DONE ===
