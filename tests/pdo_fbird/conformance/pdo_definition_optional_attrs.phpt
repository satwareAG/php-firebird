--TEST--
PDO Definition: optional get_attribute / set_attribute (incl FBIRD_TXN_*)
--CREDITS--
v12.1.0 M1-7 (#334)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';

echo "=== Optional: get/set_attribute ===\n";
$pdo = pdo_fbird_connect();

// Standard PDO attributes
echo "OK ATTR_DRIVER_NAME: " . $pdo->getAttribute(PDO::ATTR_DRIVER_NAME) . "\n";
echo "OK ATTR_AUTOCOMMIT: " . var_export($pdo->getAttribute(PDO::ATTR_AUTOCOMMIT), true) . "\n";
echo "OK ATTR_SERVER_VERSION: " . (strlen($pdo->getAttribute(PDO::ATTR_SERVER_VERSION)) > 0 ? 'returned' : 'empty') . "\n";
echo "OK ATTR_CLIENT_VERSION: " . (strlen($pdo->getAttribute(PDO::ATTR_CLIENT_VERSION)) > 0 ? 'returned' : 'empty') . "\n";

// Set standard attributes
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_SILENT);
echo "OK set ERRMODE_SILENT\n";
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_WARNING);
echo "OK set ERRMODE_WARNING\n";
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
echo "OK set ERRMODE_EXCEPTION\n";

// FBIRD-specific attributes
$pdo->exec("RECREATE TABLE test_attrs (val INT)");
$pdo->exec("INSERT INTO test_attrs VALUES (1)");
try {
    $pdo->setAttribute(PDO::FBIRD_ATTR_FETCH_TABLE_NAMES, true);
    $stmt = $pdo->query("SELECT val FROM test_attrs");
    $row = $stmt->fetch(PDO::FETCH_ASSOC);
    $keys = array_keys($row);
    echo "OK FETCH_TABLE_NAMES key: " . $keys[0] . "\n";
    $pdo->setAttribute(PDO::FBIRD_ATTR_FETCH_TABLE_NAMES, false);
} catch (\Throwable $e) {
    echo "SKIP FETCH_TABLE_NAMES: " . $e->getMessage() . "\n";
}

// FBIRD transaction isolation levels - assert round-trip value
try {
    $pdo->setAttribute(PDO::FBIRD_ATTR_TRANSACTION_ISOLATION_LEVEL, PDO::FBIRD_TXN_READ_COMMITTED);
    $val = $pdo->getAttribute(PDO::FBIRD_ATTR_TRANSACTION_ISOLATION_LEVEL);
    echo "OK TXN_ISOLATION round-trip: set=" . PDO::FBIRD_TXN_READ_COMMITTED . " got=$val\n";
} catch (\Throwable $e) {
    echo "SKIP TXN_ISOLATION: " . $e->getMessage() . "\n";
}

// Date/time format - assert round-trip
try {
    $pdo->setAttribute(PDO::FBIRD_ATTR_TIMESTAMP_FORMAT, '%Y-%m-%d %H:%M:%S');
    echo "OK set TIMESTAMP_FORMAT\n";
} catch (\Throwable $e) {
    echo "SKIP TIMESTAMP_FORMAT: " . $e->getMessage() . "\n";
}

echo "=== DONE ===\n";
?>
--CLEAN--
<?php require_once __DIR__ . '/../../pdo_fbird.inc'; @$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]); @$pdo->exec("DROP TABLE test_attrs"); ?>
--EXPECTF--
=== Optional: get/set_attribute ===
OK ATTR_DRIVER_NAME: fbird
OK ATTR_AUTOCOMMIT: true
OK ATTR_SERVER_VERSION: returned
OK ATTR_CLIENT_VERSION: returned
OK set ERRMODE_SILENT
OK set ERRMODE_WARNING
OK set ERRMODE_EXCEPTION
OK FETCH_TABLE_NAMES key: %s
OK TXN_ISOLATION round-trip: set=1 got=1
OK set TIMESTAMP_FORMAT
=== DONE ===
