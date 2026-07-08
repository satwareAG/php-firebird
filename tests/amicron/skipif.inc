<?php
/**
 * Standard SKIPIF for amicron integration tests.
 *
 * Verifies:
 *   - firebird extension is loaded
 *   - amicron-firebird Docker container is running with amicron-demo.fdb
 *   - SYSDBA/masterkey credentials work (public demo creds from amicron-software.de)
 *
 * These tests only run locally (not in CI) since the amicron DBs are private
 * (~/internal/amicron-firebird Docker container).
 *
 * Usage in --SKIPIF-- section:
 *   <?php require_once __DIR__ . '/skipif.inc'; ?>
 *
 * @package php-firebird
 * @since   12.1.0
 */

declare(strict_types=1);

if (!extension_loaded('firebird')) {
    die('skip firebird extension not loaded');
}

// amicron-demo.fdb uses SYSDBA/masterkey (public demo credentials)
$conn = @fbird_connect(
    'localhost/3050:/var/lib/firebird/data/amicron-demo.fdb',
    'SYSDBA',
    'masterkey',
    'ISO8859_1'
);

if (!$conn) {
    die('skip amicron-firebird container not available or amicron-demo.fdb not accessible');
}

fbird_close($conn);
