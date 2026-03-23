--TEST--
PDO Firebird: Service API via PDO attributes
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not loaded');
require_once __DIR__ . '/pdo_fbird.inc';
try { pdo_fbird_connect(); } catch (Throwable $e) { die('skip cannot connect: ' . $e->getMessage()); }
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

// Test 1: Service attach (auto-attaches on first service operation)
echo "=== Service Attach ===\n";
$pdo->setAttribute(PDO::FBIRD_ATTR_SERVICE_ATTACH, true);
$attached = $pdo->getAttribute(PDO::FBIRD_ATTR_SERVICE_ATTACH);
echo "attached: " . ($attached ? "true" : "false") . "\n";

// Test 2: Get server version via service API
echo "=== Server Version ===\n";
$version = $pdo->getAttribute(PDO::FBIRD_ATTR_SERVICE_SERVER_VERSION);
echo "version type: " . gettype($version) . "\n";
echo "version not empty: " . ($version ? "true" : "false") . "\n";
// Firebird version strings contain "Firebird" or "WI-" or "LI-"
echo "looks like version: " . (preg_match('/Firebird|WI-|LI-/i', $version) ? "true" : "false") . "\n";

// Test 3: Get server info (implementation)
echo "=== Server Info ===\n";
$info = $pdo->getAttribute(PDO::FBIRD_ATTR_SERVICE_SERVER_INFO);
echo "info type: " . gettype($info) . "\n";
echo "info not empty: " . ($info ? "true" : "false") . "\n";

// Test 4: Service detach
echo "=== Service Detach ===\n";
$pdo->setAttribute(PDO::FBIRD_ATTR_SERVICE_DETACH, true);
$attached = $pdo->getAttribute(PDO::FBIRD_ATTR_SERVICE_ATTACH);
echo "attached after detach: " . ($attached ? "true" : "false") . "\n";

// Test 5: Auto-reattach on server version query
echo "=== Auto-reattach ===\n";
$version2 = $pdo->getAttribute(PDO::FBIRD_ATTR_SERVICE_SERVER_VERSION);
echo "version after reattach: " . ($version2 ? "true" : "false") . "\n";

// Test 6: Constants exist
echo "=== Constants ===\n";
$constants = [
    'FBIRD_ATTR_SERVICE_ATTACH', 'FBIRD_ATTR_SERVICE_DETACH',
    'FBIRD_ATTR_SERVICE_BACKUP', 'FBIRD_ATTR_SERVICE_RESTORE',
    'FBIRD_ATTR_SERVICE_SERVER_VERSION', 'FBIRD_ATTR_SERVICE_SERVER_INFO',
    'FBIRD_ATTR_SERVICE_DB_STATS',
    'FBIRD_ATTR_SERVICE_ADD_USER', 'FBIRD_ATTR_SERVICE_MODIFY_USER',
    'FBIRD_ATTR_SERVICE_DELETE_USER',
];
$all_exist = true;
foreach ($constants as $c) {
    $full = "PDO::$c";
    if (!defined($full)) {
        echo "MISSING: $full\n";
        $all_exist = false;
    }
}
echo "all constants exist: " . ($all_exist ? "true" : "false") . "\n";

$pdo->setAttribute(PDO::FBIRD_ATTR_SERVICE_DETACH, true);
$pdo = null;
echo "Done\n";
?>
--EXPECT--
=== Service Attach ===
attached: true
=== Server Version ===
version type: string
version not empty: true
looks like version: true
=== Server Info ===
info type: string
info not empty: true
=== Service Detach ===
attached after detach: false
=== Auto-reattach ===
version after reattach: true
=== Constants ===
all constants exist: true
Done
