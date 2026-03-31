--TEST--
pdo_fbird: WRITABLE_TRANSACTION attribute (read-only vs read-write)
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

// Default should be writable
echo "Default writable: " . ($pdo->getAttribute(PDO::FBIRD_ATTR_WRITABLE_TRANSACTION) ? "yes" : "no") . "\n";

// Set to read-only
$pdo->setAttribute(PDO::FBIRD_ATTR_WRITABLE_TRANSACTION, false);
echo "After set false: " . ($pdo->getAttribute(PDO::FBIRD_ATTR_WRITABLE_TRANSACTION) ? "yes" : "no") . "\n";

// Set back to writable
$pdo->setAttribute(PDO::FBIRD_ATTR_WRITABLE_TRANSACTION, true);
echo "After set true: " . ($pdo->getAttribute(PDO::FBIRD_ATTR_WRITABLE_TRANSACTION) ? "yes" : "no") . "\n";

// Verify read-only transaction rejects writes
$pdo->exec("RECREATE TABLE wt_test (id INTEGER)");
$pdo->exec("INSERT INTO wt_test VALUES (1)");

$pdo->setAttribute(PDO::FBIRD_ATTR_WRITABLE_TRANSACTION, false);
$pdo->beginTransaction();
// Read should work
$row = $pdo->query("SELECT COUNT(*) AS cnt FROM wt_test")->fetch(PDO::FETCH_ASSOC);
echo "Read in RO txn: " . $row['CNT'] . "\n";

// Write should fail
try {
    $pdo->exec("INSERT INTO wt_test VALUES (2)");
    echo "Write in RO txn: allowed\n";
} catch (PDOException $e) {
    echo "Write in RO txn: blocked\n";
}
$pdo->rollBack();

// Cleanup with writable transaction
$pdo->setAttribute(PDO::FBIRD_ATTR_WRITABLE_TRANSACTION, true);
$pdo->exec("DROP TABLE wt_test");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
Default writable: yes
After set false: no
After set true: yes
Read in RO txn: 1
Write in RO txn: blocked
Done

--CLEAN--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();
@$pdo->exec("DROP TABLE wt_test");
unset($pdo);
?>
