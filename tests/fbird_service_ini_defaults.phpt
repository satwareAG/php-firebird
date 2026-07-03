--TEST--
fbird_service_attach() should use INI default credentials (Issue #71)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--INI--
fbird.default_user=SYSDBA
fbird.default_password=masterkey
--FILE--
<?php

require("firebird.inc");

$host = getenv('FIREBIRD_HOST') ?: 'localhost';
$user = 'SYSDBA';
$pass = 'masterkey';

echo "=== Test 1: Service attach with explicit credentials (baseline) ===\n";
$svc1 = @fbird_service_attach($host, $user, $pass);
if ($svc1) {
    echo "OK: Service attach with explicit credentials succeeded\n";
    fbird_service_detach($svc1);
} else {
    echo "FAIL: Service attach with explicit credentials failed: " . fbird_errmsg() . "\n";
}

echo "\n=== Test 2: Service attach with host only (INI user+pass fallback) ===\n";
// This tests Issue #71 fix - should fall back to fbird.default_user and fbird.default_password
$svc2 = @fbird_service_attach($host);
if ($svc2) {
    echo "OK: Service attach with INI defaults succeeded\n";

    // Verify the connection works by querying server info
    $version = @fbird_server_info($svc2, FBIRD_SVC_SERVER_VERSION);
    if ($version !== false) {
        echo "OK: Server version query succeeded\n";
    } else {
        echo "WARN: Server version query returned false\n";
    }

    fbird_service_detach($svc2);
} else {
    echo "FAIL: Service attach with INI defaults failed: " . fbird_errmsg() . "\n";
}

echo "\n=== Test 3: Service attach with empty strings (INI fallback) ===\n";
// Empty strings should trigger INI fallback
$svc3 = @fbird_service_attach($host, '', '');
if ($svc3) {
    echo "OK: Service attach with empty strings used INI defaults\n";
    fbird_service_detach($svc3);
} else {
    echo "FAIL: Service attach with empty strings failed: " . fbird_errmsg() . "\n";
}

echo "\nDone.\n";
?>
--EXPECTF--
=== Test 1: Service attach with explicit credentials (baseline) ===
OK: Service attach with explicit credentials succeeded

=== Test 2: Service attach with host only (INI user+pass fallback) ===
OK: Service attach with INI defaults succeeded
OK: Server version query succeeded

=== Test 3: Service attach with empty strings (INI fallback) ===
OK: Service attach with empty strings used INI defaults

Done.

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
