<?php

declare(strict_types=1);

/**
 * php-firebird OO API autoloader.
 *
 * Registers a PSR-4 autoloader for the Firebird\ namespace, pointing to
 * /usr/share/php/Firebird/. This file is loaded via the INI directive
 * auto_prepend_file in /etc/php/{VERSION}/mods-available/20-firebird.ini
 * so that all Firebird\* classes are available whenever the extension is loaded.
 *
 * The C extension (firebird.so) provides the internal classes:
 *   - Firebird\Connection (internal)
 *   - Firebird\Statement (internal)
 *
 * The user-defined classes in /usr/share/php/Firebird/ provide:
 *   - Firebird\Database, Firebird\TBuilder, Firebird\TransactionManager
 *   - Firebird\Batch, Firebird\BatchError, Firebird\BatchResult
 *   - Firebird\BlobId, Firebird\DbInfo
 *   - Firebird\EventPollerInterface, Firebird\EventPoller
 *   - Firebird\FiberEventPoller, Firebird\PcntlEventPoller, Firebird\ProcessEventPoller
 *
 * @internal This file is part of the php-firebird package and not meant to be
 *           called directly. It is loaded automatically via auto_prepend_file.
 */

// Jane: hardcoded path matches Debian/Ubuntu convention. For non-Debian
// packages (RPM, Alpine), the postinst script regenerates this file with
// the correct path. Alternatively, detect at runtime via dirname(__DIR__).
$firebirdClassDir = '/usr/share/php/Firebird';

// Allow override via environment variable (for testing/PIE source builds)
$envOverride = getenv('PHP_FIREBIRD_CLASS_DIR');
if ($envOverride !== false && is_dir($envOverride)) {
    $firebirdClassDir = $envOverride;
}

// Only register if the directory exists
if (!is_dir($firebirdClassDir)) {
    return;
}

spl_autoload_register(function (string $class) use ($firebirdClassDir): void {
    // Only handle Firebird\ namespace
    if (!str_starts_with($class, 'Firebird\\')) {
        return;
    }

    // PSR-4: Firebird\Database -> /usr/share/php/Firebird/Database.php
    $relativeClass = substr($class, strlen('Firebird\\'));
    $file = $firebirdClassDir . '/' . str_replace('\\', '/', $relativeClass) . '.php';

    if (is_file($file)) {
        require $file;
    }
});
