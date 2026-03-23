--TEST--
pdo_fbird: error handling modes (ERRMODE_EXCEPTION and ERRMODE_WARNING)
--SKIPIF--
<?php
if (!extension_loaded('pdo')) die('skip PDO not available');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not loaded');
require_once __DIR__ . '/pdo_fbird.inc';
try { pdo_fbird_connect(); } catch (Throwable $e) { die('skip cannot connect: ' . $e->getMessage()); }
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';

$pdo = pdo_fbird_connect();
try {
    $pdo->exec("SELECT * FROM nonexistent_table_xyz_12345");
    echo "Exception mode: should have thrown\n";
} catch (PDOException $e) {
    echo "Exception mode: caught PDOException\n";
    echo "Has message: " . (strlen($e->getMessage()) > 0 ? "yes" : "no") . "\n";
}

// Test 2: ERRMODE_WARNING
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_WARNING);
set_error_handler(function($errno, $errstr) {
    echo "Warning mode: caught warning\n";
    return true;
});
$result = $pdo->exec("SELECT * FROM nonexistent_table_xyz_12345");
restore_error_handler();
echo "Warning mode result: " . ($result === false ? "false" : "other") . "\n";

// Test 3: ERRMODE_SILENT
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_SILENT);
$result = $pdo->exec("SELECT * FROM nonexistent_table_xyz_12345");
echo "Silent mode result: " . ($result === false ? "false" : "other") . "\n";
$info = $pdo->errorInfo();
echo "Error info available: " . (count($info) >= 3 ? "yes" : "no") . "\n";

$pdo = null;
echo "Done\n";
?>
--EXPECT--
Exception mode: caught PDOException
Has message: yes
Warning mode: caught warning
Warning mode result: false
Silent mode result: false
Error info available: yes
Done
