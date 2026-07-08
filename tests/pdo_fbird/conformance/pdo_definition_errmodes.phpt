--TEST--
PDO Definition: all 3 ERRMODE modes
--CREDITS--
v12.1.0 M1-18 (#345)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';
echo "=== ERRMODE modes ===\n";

// ERRMODE_SILENT
$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]);
$r = @$pdo->exec("DELETE FROM nonexistent_silent");
echo "OK ERRMODE_SILENT: no throw, errorCode=" . $pdo->errorCode() . "\n";

// ERRMODE_WARNING
$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_WARNING]);
$r = @$pdo->exec("DELETE FROM nonexistent_warning");
echo "OK ERRMODE_WARNING: no throw, errorCode=" . $pdo->errorCode() . "\n";

// ERRMODE_EXCEPTION
$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);
try {
    $pdo->exec("DELETE FROM nonexistent_exception");
    echo "FAIL ERRMODE_EXCEPTION: should have thrown\n";
} catch (\PDOException $e) {
    echo "OK ERRMODE_EXCEPTION: threw " . $e->getCode() . "\n";
}

echo "=== DONE ===\n";
?>
--EXPECTF--
=== ERRMODE modes ===
OK ERRMODE_SILENT: no throw, errorCode=%s
OK ERRMODE_WARNING: no throw, errorCode=%s
OK ERRMODE_EXCEPTION: threw %s
=== DONE ===
