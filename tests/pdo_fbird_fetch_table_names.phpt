--TEST--
pdo_fbird: FETCH_TABLE_NAMES attribute set/get and describe_col prepend
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

// Default should be off
echo "Default: " . ($pdo->getAttribute(PDO::FBIRD_ATTR_FETCH_TABLE_NAMES) ? "on" : "off") . "\n";

// Enable
$pdo->setAttribute(PDO::FBIRD_ATTR_FETCH_TABLE_NAMES, true);
echo "After enable: " . ($pdo->getAttribute(PDO::FBIRD_ATTR_FETCH_TABLE_NAMES) ? "on" : "off") . "\n";

// Disable
$pdo->setAttribute(PDO::FBIRD_ATTR_FETCH_TABLE_NAMES, false);
echo "After disable: " . ($pdo->getAttribute(PDO::FBIRD_ATTR_FETCH_TABLE_NAMES) ? "on" : "off") . "\n";

// Verify describe_col prepends table name
pdo_fbird_with_table($pdo, 'FTN_T', 'ID INTEGER, NAME VARCHAR(50)', function($pdo) {
    $pdo->exec("INSERT INTO FTN_T VALUES (1, 'Alice')");

    // Without FETCH_TABLE_NAMES
    $stmt = $pdo->query("SELECT ID, NAME FROM FTN_T");
    $row = $stmt->fetch(PDO::FETCH_ASSOC);
    echo "cols without: " . implode(', ', array_keys($row)) . "\n";

    // With FETCH_TABLE_NAMES
    $pdo->setAttribute(PDO::FBIRD_ATTR_FETCH_TABLE_NAMES, true);
    $stmt = $pdo->query("SELECT ID, NAME FROM FTN_T");
    $row = $stmt->fetch(PDO::FETCH_ASSOC);
    echo "cols with: " . implode(', ', array_keys($row)) . "\n";
});

$pdo = null;
echo "Done\n";
?>
--EXPECT--
Default: off
After enable: on
After disable: off
cols without: ID, NAME
cols with: FTN_T.ID, FTN_T.NAME
Done
