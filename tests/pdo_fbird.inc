<?php
/**
 * PDO fbird test helper — connects to the shared test database directly.
 * Unlike firebird.inc, this does NOT create/drop temporary databases,
 * avoiding shutdown-order crashes when PDO connections outlive fbird resources.
 */
require_once __DIR__ . '/config.inc';

$dbEnv = getenv('FIREBIRD_DATABASE');
$db = ($dbEnv !== false && $dbEnv !== '') ? $dbEnv : '/firebird/data/test.fdb';

$dsn_base = $host . ':' . $db;
$dsn = "fbird:host={$host};dbname={$db};charset=UTF8";

/**
 * Get a PDO connection to the test database.
 */
function pdo_fbird_connect(array $options = []): PDO {
    global $dsn, $user, $password;
    $defaults = [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION];
    return new PDO($dsn, $user, $password, $options + $defaults);
}

/**
 * Create a temporary test table, execute callback, then drop it.
 * Ensures cleanup even on failure.
 */
function pdo_fbird_with_table(PDO $pdo, string $tableName, string $ddl, callable $fn): void {
    $pdo->exec("RECREATE TABLE {$tableName} ({$ddl})");
    try {
        $fn($pdo, $tableName);
    } finally {
        try { $pdo->exec("DROP TABLE {$tableName}"); } catch (Throwable $e) {}
    }
}
