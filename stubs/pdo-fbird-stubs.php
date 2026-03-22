<?php

/**
 * PHP Firebird PDO Driver Stubs (pdo_fbird)
 *
 * Stub declarations for the pdo_fbird extension — a separate PDO driver
 * using the "fbird:" DSN prefix. This avoids collision with PHP's bundled
 * pdo_firebird driver ("firebird:" DSN).
 *
 * DSN format:
 *   fbird:host=<host>;dbname=<path>[;charset=<charset>][;dialect=<1|3>][;role=<role>]
 *
 * Example:
 *   $pdo = new PDO('fbird:host=localhost;dbname=/var/lib/firebird/data/mydb.fdb',
 *                  'SYSDBA', 'masterkey');
 *
 * @package   php-firebird-stubs
 * @version   8.0.0
 * @author    satware AG <info@satware.com>
 * @copyright 2025 satware AG
 * @license   PHP-3.01 https://www.php.net/license/3_01.txt
 * @link      https://github.com/satwareAG/php-firebird
 *
 * @since 8.0.0
 */

declare(strict_types=1);

if (extension_loaded('pdo_fbird')) {
    return;
}

/**
 * PDO driver constants for the fbird: DSN prefix.
 *
 * These constants are registered on the PDO class when pdo_fbird is loaded.
 */
final class PDO_FBIRD_Constants
{
    /**
     * Firebird-specific attribute: transaction flags.
     * Use with PDO::setAttribute(PDO::FBIRD_ATTR_TRANSACTION_FLAGS, PHP_FBIRD_*).
     */
    public const FBIRD_ATTR_TRANSACTION_FLAGS = 1000;

    /**
     * Firebird-specific attribute: SQL dialect (1 or 3).
     * Use with PDO::setAttribute(PDO::FBIRD_ATTR_DIALECT, 3).
     */
    public const FBIRD_ATTR_DIALECT = 1001;

    /**
     * Firebird-specific attribute: character set.
     * Use with PDO::setAttribute(PDO::FBIRD_ATTR_CHARSET, 'UTF8').
     */
    public const FBIRD_ATTR_CHARSET = 1002;

    private function __construct() {}
}

/*
 * The pdo_fbird driver registers the following PDO::FBIRD_ATTR_* constants
 * directly on the PDO class at extension load time. They are listed here
 * for IDE/static-analysis awareness only.
 *
 * Usage example:
 *
 *   $pdo = new PDO('fbird:host=localhost;dbname=/data/mydb.fdb', 'SYSDBA', 'masterkey', [
 *       PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
 *       PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
 *       PDO::FBIRD_ATTR_DIALECT      => 3,
 *       PDO::FBIRD_ATTR_CHARSET      => 'UTF8',
 *   ]);
 *
 *   // Transactions
 *   $pdo->beginTransaction();
 *   $pdo->exec("INSERT INTO t (col) VALUES (1)");
 *   $pdo->commit();
 *
 *   // Prepared statements with positional parameters
 *   $stmt = $pdo->prepare("SELECT * FROM t WHERE id = ?");
 *   $stmt->execute([42]);
 *   $rows = $stmt->fetchAll(PDO::FETCH_ASSOC);
 *
 *   // Named parameters
 *   $stmt = $pdo->prepare("INSERT INTO t (name, val) VALUES (:name, :val)");
 *   $stmt->execute([':name' => 'foo', ':val' => 123]);
 *
 * Supported PDO features:
 *   - PDO::ATTR_SERVER_VERSION  (via getAttribute)
 *   - PDO::ATTR_AUTOCOMMIT      (default: true)
 *   - PDO::ATTR_ERRMODE         (SILENT, WARNING, EXCEPTION)
 *   - PDO::PARAM_INT, PDO::PARAM_STR, PDO::PARAM_NULL, PDO::PARAM_LOB
 *   - PDO::FETCH_ASSOC, PDO::FETCH_NUM, PDO::FETCH_BOTH, PDO::FETCH_OBJ
 *   - Positional (?) and named (:name) parameter binding
 *   - BLOB/LOB support via PDO::PARAM_LOB streams
 *   - GDS error code → SQLSTATE mapping
 *
 * Unsupported / not yet implemented:
 *   - Scrollable cursors (PDO::CURSOR_SCROLL)
 *   - PDO::lastInsertId() — use fbird_gen_id() or RETURNING clause instead
 *   - Multiple active result sets on a single connection
 */
