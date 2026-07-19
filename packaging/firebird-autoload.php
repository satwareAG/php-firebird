<?php

declare(strict_types=1);

/**
 * php-firebird OO API autoloader (optional helper).
 *
 * Registers a PSR-4 autoloader for the Firebird\ namespace.
 *
 * WHEN IS THIS NEEDED?
 *
 * The C extension (firebird.so) provides the internal classes:
 *   - Firebird\Connection (internal, no PHP file needed)
 *   - Firebird\Statement (internal, no PHP file needed)
 *
 * The user-defined classes in this directory provide:
 *   - Firebird\Database, Firebird\TBuilder, Firebird\TransactionManager
 *   - Firebird\Batch, Firebird\BatchError, Firebird\BatchResult
 *   - Firebird\BlobId, Firebird\DbInfo
 *   - Firebird\EventPollerInterface, Firebird\EventPoller
 *   - Firebird\FiberEventPoller, Firebird\PcntlEventPoller, Firebird\ProcessEventPoller
 *   - Firebird\functions (fbird_query_params, fbird_trans_begin, etc.)
 *
 * COMPOSER USERS: Autoloading is handled automatically by composer.json:
 *   "autoload": { "psr-4": { "Firebird\\": "src/Firebird/" } }
 *   Do NOT require this file - Composer's autoloader already handles it.
 *
 * NON-COMPOSER USERS (e.g., native package installs without Composer):
 *   require_once '/usr/share/php/Firebird/autoload.php';
 *
 * WHY NOT auto_prepend_file?
 *
 * The auto_prepend_file INI directive does not work reliably in CLI mode
 * (it's ignored by php -r, php -m, and some SAPI configurations). Instead,
 * users who need the OOP API should either:
 *   1. Use Composer (recommended), or
 *   2. Add `require_once '/usr/share/php/Firebird/autoload.php';` to their
 *      bootstrap file
 *
 * @internal This file is part of the php-firebird package.
 */

$firebirdClassDir = dirname(__FILE__);

// Allow override via environment variable (for testing/PIE source builds)
$envOverride = getenv('PHP_FIREBIRD_CLASS_DIR');
if ($envOverride !== false && is_dir($envOverride)) {
    $firebirdClassDir = $envOverride;
}

if (!is_dir($firebirdClassDir)) {
    return;
}

spl_autoload_register(function (string $class) use ($firebirdClassDir): void {
    // Only handle Firebird\ namespace
    if (!str_starts_with($class, 'Firebird\\')) {
        return;
    }

    // PSR-4: Firebird\Database -> Database.php
    $relativeClass = substr($class, strlen('Firebird\\'));
    $file = $firebirdClassDir . '/' . str_replace('\\', '/', $relativeClass) . '.php';

    if (is_file($file)) {
        require $file;
    }
});
