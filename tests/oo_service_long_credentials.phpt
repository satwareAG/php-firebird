--TEST--
Security: Firebird\Service rejects oversized credentials (C2 fix)
--SKIPIF--
<?php
if (!extension_loaded("firebird")) die("skip firebird extension not available");
if (!class_exists('Firebird\Service')) die("skip Firebird\\Service class not available");
?>
--FILE--
<?php

// Test 1: Username > 255 bytes throws ServiceException
echo "Test 1: Long username\n";
try {
    $svc = new Firebird\Service("localhost", str_repeat("A", 256), "password");
    echo "ERROR: Should have thrown\n";
} catch (Firebird\ServiceException $e) {
    echo "Caught: " . $e->getMessage() . "\n";
}

// Test 2: Password > 255 bytes throws ServiceException
echo "Test 2: Long password\n";
try {
    $svc = new Firebird\Service("localhost", "SYSDBA", str_repeat("B", 256));
    echo "ERROR: Should have thrown\n";
} catch (Firebird\ServiceException $e) {
    echo "Caught: " . $e->getMessage() . "\n";
}

// Test 3: Host > 243 bytes throws ServiceException
echo "Test 3: Long hostname\n";
try {
    $svc = new Firebird\Service(str_repeat("H", 244), "SYSDBA", "masterkey");
    echo "ERROR: Should have thrown\n";
} catch (Firebird\ServiceException $e) {
    echo "Caught: " . $e->getMessage() . "\n";
}

// Test 4: Valid short credentials pass validation (may fail at connect, that's OK)
echo "Test 4: Normal credentials\n";
try {
    $svc = new Firebird\Service("localhost", "SYSDBA", "masterkey");
    echo "Constructed OK (connect may or may not succeed)\n";
} catch (Firebird\ServiceException $e) {
    // Connection failure is OK - we're testing validation, not connectivity
    echo "Connect error (validation passed): " . $e->getMessage() . "\n";
}

echo "Done\n";
?>
--EXPECTF--
Test 1: Long username
Caught: Username exceeds maximum SPB length of 255 bytes
Test 2: Long password
Caught: Password exceeds maximum SPB length of 255 bytes
Test 3: Long hostname
Caught: Hostname exceeds maximum length
Test 4: Normal credentials
%s
Done
