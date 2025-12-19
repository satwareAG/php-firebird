<?php

declare(strict_types=1);

/**
 * php-firebird: OO Database Wrapper
 *
 * Provides an object-oriented interface wrapping the procedural fbird_* functions.
 *
 * @package   Firebird
 * @author    Michael Wegener <mw@satware.com>
 * @copyright satware AG 2025
 * @license   PHP License (same as PHP itself)
 */

namespace Firebird;

/**
 * Object-oriented wrapper for Firebird database connections.
 *
 * This class provides a modern OO interface while wrapping the procedural
 * fbird_* functions. Full backward compatibility with procedural code is
 * maintained - you can use both styles interchangeably.
 *
 * Usage:
 * ```php
 * use Firebird\Database;
 *
 * // Connect
 * $db = Database::connect('localhost:/var/lib/firebird/test.fdb', 'SYSDBA', 'masterkey');
 *
 * // Start transaction with builder
 * $trans = $db->transaction()
 *     ->readCommitted()
 *     ->wait()
 *     ->start();
 *
 * // Execute query
 * $result = $db->query("SELECT * FROM users WHERE id = ?", [1]);
 *
 * // Or using the procedural function with the resource
 * $result = fbird_query($db->getResource(), "SELECT * FROM users");
 * ```
 *
 * @see https://github.com/satwareAG/php-firebird
 */
class Database
{
    private mixed $resource;
    private bool $persistent;
    private string $database;
    private ?string $username;

    /**
     * Private constructor - use factory methods.
     */
    private function __construct(mixed $resource, string $database, ?string $username, bool $persistent)
    {
        $this->resource = $resource;
        $this->database = $database;
        $this->username = $username;
        $this->persistent = $persistent;
    }

    /**
     * Connect to a Firebird database.
     *
     * @param string $database Database path (host:/path/to/db.fdb or /path/to/db.fdb)
     * @param string|null $username Username (defaults to INI setting)
     * @param string|null $password Password (defaults to INI setting)
     * @param string|null $charset Character set (defaults to INI setting)
     * @param int $buffers Number of database buffers (0 = server default)
     * @param int $dialect SQL dialect (1, 2, or 3)
     * @param string|null $role SQL role
     * @return self
     * @throws \Exception If connection fails
     */
    public static function connect(
        string $database,
        ?string $username = null,
        ?string $password = null,
        ?string $charset = null,
        int $buffers = 0,
        int $dialect = 3,
        ?string $role = null
    ): self {
        $resource = @fbird_connect($database, $username, $password, $charset, $buffers, $dialect, $role);

        if ($resource === false) {
            throw new \Exception(fbird_errmsg() ?: 'Failed to connect to database');
        }

        return new self($resource, $database, $username, false);
    }

    /**
     * Create a persistent connection to a Firebird database.
     *
     * @param string $database Database path
     * @param string|null $username Username
     * @param string|null $password Password
     * @param string|null $charset Character set
     * @param int $buffers Number of database buffers
     * @param int $dialect SQL dialect
     * @param string|null $role SQL role
     * @return self
     * @throws \Exception If connection fails
     */
    public static function pconnect(
        string $database,
        ?string $username = null,
        ?string $password = null,
        ?string $charset = null,
        int $buffers = 0,
        int $dialect = 3,
        ?string $role = null
    ): self {
        $resource = @fbird_pconnect($database, $username, $password, $charset, $buffers, $dialect, $role);

        if ($resource === false) {
            throw new \Exception(fbird_errmsg() ?: 'Failed to create persistent connection');
        }

        return new self($resource, $database, $username, true);
    }

    /**
     * Wrap an existing connection resource.
     *
     * @param mixed $resource Existing fbird connection resource
     * @param string $database Database path (for reference)
     * @return self
     */
    public static function fromResource(mixed $resource, string $database = ''): self
    {
        return new self($resource, $database, null, false);
    }

    /**
     * Get the underlying connection resource.
     *
     * Useful for interoperability with procedural fbird_* functions.
     *
     * @return mixed
     */
    public function getResource(): mixed
    {
        return $this->resource;
    }

    /**
     * Check if this is a persistent connection.
     *
     * @return bool
     */
    public function isPersistent(): bool
    {
        return $this->persistent;
    }

    /**
     * Get the database path.
     *
     * @return string
     */
    public function getDatabasePath(): string
    {
        return $this->database;
    }

    /**
     * Start a new transaction with the builder pattern.
     *
     * @return TBuilder Transaction builder
     */
    public function transaction(): TBuilder
    {
        return TBuilder::create()->connection($this->resource);
    }

    /**
     * Start a default transaction.
     *
     * @return Transaction
     * @throws \Exception If transaction start fails
     */
    public function beginTransaction(): Transaction
    {
        return $this->transaction()->start();
    }

    /**
     * Execute a query and return the result.
     *
     * @param string $sql SQL query
     * @param array $params Parameters for prepared statement
     * @param int $bindTypes Bind type flags (optional)
     * @return mixed Query result or false on failure
     */
    public function query(string $sql, array $params = [], int $bindTypes = 0): mixed
    {
        if (empty($params)) {
            return fbird_query($this->resource, $sql);
        }

        $args = array_merge([$this->resource, $sql], $params);
        return fbird_query(...$args);
    }

    /**
     * Execute a query within a transaction.
     *
     * @param mixed $transaction Transaction resource or Transaction object
     * @param string $sql SQL query
     * @param array $params Parameters
     * @return mixed
     */
    public function queryWithTransaction(mixed $transaction, string $sql, array $params = []): mixed
    {
        $trans = $transaction instanceof Transaction ? $transaction->getResource() : $transaction;

        if (empty($params)) {
            return fbird_query($this->resource, $trans, $sql);
        }

        $args = array_merge([$this->resource, $trans, $sql], $params);
        return fbird_query(...$args);
    }

    /**
     * Prepare a statement.
     *
     * @param string $sql SQL statement
     * @return mixed Statement resource
     */
    public function prepare(string $sql): mixed
    {
        return fbird_prepare($this->resource, $sql);
    }

    /**
     * Execute a prepared statement.
     *
     * @param mixed $statement Statement resource
     * @param array $params Parameters
     * @return mixed Query result
     */
    public function execute(mixed $statement, array $params = []): mixed
    {
        if (empty($params)) {
            return fbird_execute($statement);
        }

        return fbird_execute($statement, ...$params);
    }

    /**
     * Get the number of affected rows from the last query.
     *
     * @return int
     */
    public function affectedRows(): int
    {
        return fbird_affected_rows($this->resource);
    }

    /**
     * Create a BLOB for writing.
     *
     * @param mixed|null $transaction Transaction resource (optional)
     * @return mixed BLOB handle
     */
    public function createBlob(mixed $transaction = null): mixed
    {
        if ($transaction !== null) {
            $trans = $transaction instanceof Transaction ? $transaction->getResource() : $transaction;
            return fbird_blob_create($this->resource, $trans);
        }
        return fbird_blob_create($this->resource);
    }

    /**
     * Open a BLOB for reading.
     *
     * @param mixed $transaction Transaction resource
     * @param string $blobId BLOB ID
     * @return mixed BLOB handle
     */
    public function openBlob(mixed $transaction, string $blobId): mixed
    {
        $trans = $transaction instanceof Transaction ? $transaction->getResource() : $transaction;
        return fbird_blob_open($this->resource, $trans, $blobId);
    }

    /**
     * Get the next generator/sequence value.
     *
     * @param string $generator Generator name
     * @param int $increment Increment value (default 1)
     * @return int|string Next value (string for large values)
     */
    public function genId(string $generator, int $increment = 1): int|string
    {
        return fbird_gen_id($generator, $increment, $this->resource);
    }

    /**
     * Get database information.
     *
     * @return DbInfo
     */
    public function getInfo(): DbInfo
    {
        return DbInfo::fromConnection($this->resource);
    }

    /**
     * Close the database connection.
     *
     * @return bool
     */
    public function close(): bool
    {
        $result = fbird_close($this->resource);
        $this->resource = null;
        return $result;
    }

    /**
     * Check if connection is still valid.
     *
     * @return bool
     */
    public function isConnected(): bool
    {
        return $this->resource !== null && is_resource($this->resource);
    }

    /**
     * Get the last error message.
     *
     * @return string|null
     */
    public static function getLastError(): ?string
    {
        $msg = fbird_errmsg();
        return $msg ?: null;
    }

    /**
     * Get the last error code.
     *
     * @return int|null
     */
    public static function getLastErrorCode(): ?int
    {
        $code = fbird_errcode();
        return $code !== false ? $code : null;
    }

    /**
     * Destructor - closes non-persistent connections.
     */
    public function __destruct()
    {
        if ($this->resource !== null && !$this->persistent) {
            @fbird_close($this->resource);
        }
    }
}
