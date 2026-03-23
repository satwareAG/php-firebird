--TEST--
pdo_fbird: DATE/TIME/TIMESTAMP formatting and custom format attributes
--SKIPIF--
<?php
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not loaded');
require_once __DIR__ . '/pdo_fbird.inc';
try { pdo_fbird_connect(); } catch (Exception $e) { die('skip cannot connect'); }
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();

pdo_fbird_with_table($pdo, 'DT_TEST', 'D DATE, T TIME, TS TIMESTAMP', function($pdo) {
    $pdo->exec("INSERT INTO DT_TEST VALUES ('2026-03-23', '14:30:00', '2026-03-23 14:30:45')");

    // Default formats
    $row = $pdo->query("SELECT D, T, TS FROM DT_TEST")->fetch(PDO::FETCH_NUM);
    echo "date: " . $row[0] . "\n";
    echo "time: " . $row[1] . "\n";
    echo "ts: " . $row[2] . "\n";

    // Custom date format
    $pdo->setAttribute(PDO::FBIRD_ATTR_DATE_FORMAT, '%d/%m/%Y');
    $row = $pdo->query("SELECT D FROM DT_TEST")->fetch(PDO::FETCH_NUM);
    echo "custom date: " . $row[0] . "\n";

    // Custom time format
    $pdo->setAttribute(PDO::FBIRD_ATTR_TIME_FORMAT, '%H.%M.%S');
    $row = $pdo->query("SELECT T FROM DT_TEST")->fetch(PDO::FETCH_NUM);
    echo "custom time: " . $row[0] . "\n";

    // Custom timestamp format
    $pdo->setAttribute(PDO::FBIRD_ATTR_TIMESTAMP_FORMAT, '%Y%m%d %H%M%S');
    $row = $pdo->query("SELECT TS FROM DT_TEST")->fetch(PDO::FETCH_NUM);
    echo "custom ts: " . $row[0] . "\n";
});

$pdo = null;
echo "Done\n";
?>
--EXPECT--
date: 2026-03-23
time: 14:30:00
ts: 2026-03-23 14:30:45
custom date: 23/03/2026
custom time: 14.30.00
custom ts: 20260323 143045
Done
