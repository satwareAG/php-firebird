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

// Enable persistent
try {
    $pdo1->setAttribute(PDO::ATTR_PERSISTENT, true);
    $is_pers = $pdo1->getAttribute(PDO::ATTR_PERSISTENT);
    echo "OK set persistent: " . var_export($is_pers, true) . "\n";
} catch (\Throwable $e) {
    echo "SKIP persistent: " . $e->getMessage() . "\n";
}

// Close and reopen
unset($pdo1);
echo "OK unset (no crash)\n";

echo "=== DONE ===\n";
?>
--EXPECTF--
=== Optional: persistent ===
OK default persistent: false
%s
OK unset (no crash)
=== DONE ===
