<?php

/**
 * PHP Firebird Extension Class Stubs
 *
 * This file provides stub declarations for classes provided by the php-firebird
 * C extension. These are used by static analysis tools and IDEs.
 *
 * @package   php-firebird-stubs
 * @version   12.1.0
 * @author    satware AG <info@satware.com>
 * @copyright 2025 satware AG
 * @license   PHP-3.01 https://www.php.net/license/3_01.txt
 * @link      https://github.com/satwareAG/php-firebird
 *
 * @since 7.0.0
 */

declare(strict_types=1);

namespace Firebird;

if (extension_loaded('fbird')) {
    return;
}

/** Opaque event handle returned by fbird_set_event_handler(). @since 7.0.0 */
final class Event
{
    private function __construct() {}

    /**
     * Block until an event fires or timeout expires.
     * @param float $timeout Timeout in seconds (-1.0 = block forever, 0.0 = return immediately)
     * @return bool True if event fired, false on timeout/error
     * @since 12.0.0
     */
    public function wait(float $timeout = -1.0): bool {}

    /**
     * Cancel a pending event wait.
     * @return bool True on success, false if already cancelled
     * @since 12.0.0
     */
    public function cancel(): bool {}

    /**
     * Get the name of the last event that fired.
     * Returns the first registered event name if none fired yet.
     * @return string Event name
     * @since 12.0.0
     */
    public function getName(): string {}

    /**
     * Get the number of times the event callback has been invoked.
     * @return int Callback invocation count
     * @since 12.0.0
     */
    public function getCount(): int {}
}

/**
 * Base exception for all Firebird errors (thrown when exception mode is enabled).
 * @since 7.0.0
 */
class Exception extends \RuntimeException
{
    protected string $sqlState = '00000';
    public function getSqlState(): string { return $this->sqlState; }
}

/** Exception for database connection errors. @since 8.0.0 */
class DatabaseException extends Exception {}

/** Exception for transaction errors. @since 8.0.0 */
class TransactionException extends Exception {}

/**
 * Native OOP interface to a Firebird database connection.
 * @since 8.0.0
 */
class Connection
{
    /**
     * @param string $database  Database path or host:path.
     * @param string $username  Database user name.
     * @param string $password  Database password.
     * @param string $charset   Character set (default: UTF8).
     * @param int    $buffers   Cache buffer count.
     * @param int    $dialect   SQL dialect (1 or 3).
     * @param string $role      SQL role name.
     * @param bool   $persistent Use persistent connection.
     * @throws DatabaseException
     */
    public function __construct(
        string $database,
        string $username,
        string $password,
        string $charset = 'UTF8',
        int $buffers = 0,
        int $dialect = 3,
        string $role = '',
        bool $persistent = false
    ) {}

    /** Close the connection. */
    public function close(): bool { return true; }

    /** Check whether the connection is alive. */
    public function isConnected(): bool { return true; }

    /** Ping the server. */
    public function ping(): bool { return true; }

    /**
     * Begin a new transaction.
     * @param int $flags PHP_FBIRD_* transaction flags.
     * @throws TransactionException
     */
    public function beginTransaction(int $flags = 0): Transaction {}

    /**
     * Prepare a SQL statement.
     * @param string $sql SQL text.
     * @param Transaction $transaction Transaction context (required).
     * @throws Exception
     */
    public function prepare(string $sql, \Firebird\Transaction $transaction): Statement {}

    /** Set statement execution timeout in milliseconds (FB 4.0+, 0 = no timeout). */
    public function setStatementTimeout(int $milliseconds): bool { return true; }

    /** Get statement execution timeout in milliseconds. */
    public function getStatementTimeout(): int { return 0; }

    /** Set connection idle timeout in seconds (FB 4.0+, 0 = no timeout). */
    public function setIdleTimeout(int $seconds): bool { return true; }

    /** Get connection idle timeout in seconds. */
    public function getIdleTimeout(): int { return 0; }
}

/**
 * Represents an active Firebird transaction.
 * Obtained via Connection::beginTransaction().
 * @since 8.0.0
 */
class Transaction
{
    private function __construct() {}

    /** Commit the transaction. @throws TransactionException */
    public function commit(): bool { return true; }

    /** Roll back the transaction. @throws TransactionException */
    public function rollback(): bool { return true; }

    /** Check whether the transaction is still active. */
    public function isActive(): bool { return true; }
}

/**
 * Represents a prepared Firebird SQL statement.
 * Obtained via Connection::prepare().
 * @since 8.0.0
 */
class Statement
{
    private function __construct() {}

    /**
     * Execute the statement.
     * @param Transaction $transaction Active transaction context
     * @return ResultSet ResultSet for SELECT, DML, and DDL.
     * @throws Exception
     */
    public function execute(\Firebird\Transaction $transaction): ResultSet {}
}

/**
 * Result set of a Firebird SELECT query.
 * Obtained via Statement::execute().
 * @since 8.0.0
 */
class ResultSet
{
    private function __construct() {}

    /**
     * Fetch the next row as an associative array.
     * @return array<string,mixed>|false Row data or false when exhausted.
     */
    public function fetch(): array|false {}

    /** Close the cursor. */
    public function close(): bool { return true; }
}

/**
 * Native OOP interface to Firebird BLOB columns.
 * @since 8.0.0
 */
class Blob
{
    private function __construct() {}

    /**
     * Create a new BLOB for writing.
     * @throws Exception
     */
    public static function create(Connection $connection, Transaction $transaction): static {}

    /**
     * Open an existing BLOB for reading.
     * @param string $blobId BLOB quad identifier.
     * @throws Exception
     */
    public static function open(Connection $connection, Transaction $transaction, string $blobId): static {}

    /** Write data to the BLOB. @throws Exception */
    public function write(string $data): bool { return true; }

    /**
     * Read a segment from the BLOB.
     * @return string|false Data or false at end.
     */
    public function read(int $length = 8192): string|false {}

    /** Close the BLOB handle. */
    public function close(): bool { return true; }

    /** Get the BLOB identifier string. */
    public function getId(): string { return ''; }
}

/**
 * Native OOP interface to the Firebird Services API.
 * @since 8.0.0
 */
class Service
{
    /**
     * Attach to the Firebird Services Manager.
     * @throws Exception
     */
    public function __construct(
        string $host = 'localhost',
        string $username = 'SYSDBA',
        string $password = 'masterkey'
    ) {}

    /** Detach from the Services Manager. */
    public function detach(): bool { return true; }

    /** Check whether the service handle is attached. */
    public function isAttached(): bool { return true; }

    /**
     * Get the Firebird server version string.
     * @throws Exception
     */
    public function getServerVersion(): string { return ''; }
}

/**
 * Opaque batch handle returned by fbird_batch_create().
 *
 * This is an internal opaque object wrapping a Firebird IBatch resource.
 * It is not the same as the userland Firebird\Batch wrapper class.
 * @since 11.0.0
 */
final class BatchHandle
{
    public function getBlobAlignment(): int|false {}
    public function setDefaultBpb(string $bpb): bool {}
    public function cancel(): bool {}
    /** @return array<string, int>|false */
    public function execute(): array|false {}
    public function add(mixed ...$args): bool {}
    public function addBlob(string $data, int $blob_type = 0): string|false {}
}

/**
 * Container for DECFLOAT(16) and DECFLOAT(34) values from Firebird 4.0+.
 *
 * Preserves full decimal floating-point precision without PHP float coercion.
 * Use __toString() for display, rawBytes() for binary access.
 * Arithmetic operations are not yet supported (tracked separately).
 *
 * @since 13.0.0
 */
final class DecFloat
{
    /**
     * Construct from a string representation (e.g., "3.14159").
     * Automatically selects DEC34 if the value fits, otherwise DEC16.
     */
    public function __construct(string $value) {}

    /**
     * Static factory: create from a string representation.
     */
    public static function fromString(string $value): static {}

    /**
     * Returns the string representation of the DECFLOAT value.
     * @return non-empty-string
     */
    public function __toString(): string { return ''; }

    /**
     * Returns the precision of this DECFLOAT value (16 or 34).
     */
    public function toPrecision(): int { return 0; }

    /**
     * Returns the raw binary representation (8 bytes for DEC16, 16 bytes for DEC34).
     */
    public function rawBytes(): string { return ''; }
}
