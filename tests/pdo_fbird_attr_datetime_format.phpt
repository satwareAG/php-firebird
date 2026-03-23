--TEST--
pdo_fbird: date/time/timestamp format attributes set/get
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

// Check defaults
echo "Date default: " . $pdo->getAttribute(PDO::FBIRD_ATTR_DATE_FORMAT) . "\n";
echo "Time default: " . $pdo->getAttribute(PDO::FBIRD_ATTR_TIME_FORMAT) . "\n";
echo "Timestamp default: " . $pdo->getAttribute(PDO::FBIRD_ATTR_TIMESTAMP_FORMAT) . "\n";

// Set custom formats
$pdo->setAttribute(PDO::FBIRD_ATTR_DATE_FORMAT, "%d.%m.%Y");
$pdo->setAttribute(PDO::FBIRD_ATTR_TIME_FORMAT, "%H-%M-%S");
$pdo->setAttribute(PDO::FBIRD_ATTR_TIMESTAMP_FORMAT, "%d/%m/%Y %H:%M");

echo "Date custom: " . $pdo->getAttribute(PDO::FBIRD_ATTR_DATE_FORMAT) . "\n";
echo "Time custom: " . $pdo->getAttribute(PDO::FBIRD_ATTR_TIME_FORMAT) . "\n";
echo "Timestamp custom: " . $pdo->getAttribute(PDO::FBIRD_ATTR_TIMESTAMP_FORMAT) . "\n";

$pdo = null;
echo "Done\n";
?>
--EXPECT--
Date default: %Y-%m-%d
Time default: %H:%M:%S
Timestamp default: %Y-%m-%d %H:%M:%S
Date custom: %d.%m.%Y
Time custom: %H-%M-%S
Timestamp custom: %d/%m/%Y %H:%M
Done
