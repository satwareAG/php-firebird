<?php
/**
 * Standard SKIPIF for PDO conformance tests (M1 milestone).
 *
 * Verifies:
 *   - PDO extension is loaded
 *   - 'fbird' is registered as a PDO driver
 *   - FBIRD_TXN_* constants are registered (pdo_fbird loaded after firebird.so)
 *   - A test database connection can be established
 *
 * Usage in --SKIPIF-- section:
 *   <?php require_once __DIR__ . '/skipif.inc'; ?>
 *
 * @package php-firebird
 * @since   12.1.0
 */

declare(strict_types=1);

// PDO must be loaded
if (!extension_loaded('pdo')) {
    die('skip PDO extension not loaded');
}

// pdo_fbird driver must be registered
if (!in_array('fbird', PDO::getAvailableDrivers(), true)) {
    die('skip pdo_fbird driver not registered');
}

// FBIRD_TXN_* constants prove both firebird.so AND pdo_fbird.so are loaded
// (pdo_fbird depends on firebird.so per v12.0.0 breaking change)
if (!defined('PDO::FBIRD_TXN_READ_COMMITTED')) {
    die('skip FBIRD_TXN_* constants not registered (firebird.so not loaded before pdo_fbird.so?)');
}

// Try to establish a test connection
require_once __DIR__ . '/../../pdo_fbird.inc';

try {
    $pdo = pdo_fbird_connect();
    unset($pdo);
} catch (Throwable $e) {
    die('skip cannot connect to test database: ' . $e->getMessage());
}
