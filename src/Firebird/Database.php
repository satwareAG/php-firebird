<?php

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

declare(strict_types=1);

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
     * Get the username used for this connection (if provided).
     *
     * This exists mainly for diagnostics/introspection and to keep the property
     * meaningfully "read" for static analysis.
     */
    public function getUsername(): ?string
    {
        return $this->username;
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
     * @return TransactionManager
     * @throws \Exception If transaction start fails
     */
    public function beginTransaction(): TransactionManager
    {
        return $this->transaction()->start();
    }

    /**
     * Execute a query and return the result.
     *
     * @param string $sql SQL query
     * @param array<int, mixed> $params Parameters for prepared statement
     * @param int $bindTypes Bind type flags (optional)
     * @return mixed Query result or false on failure
     */
    public function query(string $sql, array $params = [], int $bindTypes = 0): mixed
    {
        return fbird_query_params($this->resource, $sql, $params);
    }

    /**
     * Execute a query within a transaction.
     *
     * @param mixed $transaction Transaction resource or TransactionManager object
     * @param string $sql SQL query
     * @param array<int, mixed> $params Parameters
     * @return mixed
     */
    public function queryWithTransaction(mixed $transaction, string $sql, array $params = []): mixed
    {
        $trans = $transaction instanceof TransactionManager ? $transaction->getResource() : $transaction;

        return fbird_query_params_tx($this->resource, $trans, $sql, $params);
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
     * @param array<int, mixed> $params Parameters
     * @return mixed Query result
     */
    public function execute(mixed $statement, array $params = []): mixed
    {
        return fbird_execute_params($statement, $params);
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
     * The fbird_blob_create() function accepts a link or transaction resource.
     * When a transaction is passed, the extension resolves both link and trans.
     *
     * @param mixed|null $transaction Transaction resource (optional)
     * @return mixed BLOB handle
     */
    public function createBlob(mixed $transaction = null): mixed
    {
        if ($transaction !== null) {
            // Pass transaction resource - extension resolves link from it
            $trans = $transaction instanceof TransactionManager ? $transaction->getResource() : $transaction;
            return fbird_blob_create($trans);
        }
        return fbird_blob_create($this->resource);
    }

    /**
     * Open a BLOB for reading.
     *
     * The fbird_blob_open() function accepts (link, blob_id) or (trans, blob_id).
     * When a transaction is passed, the extension resolves both link and trans.
     *
     * @param mixed $transaction Transaction resource
     * @param string $blobId BLOB ID
     * @return mixed BLOB handle
     */
    public function openBlob(mixed $transaction, string $blobId): mixed
    {
        // Pass transaction resource - extension resolves link from it
        $trans = $transaction instanceof TransactionManager ? $transaction->getResource() : $transaction;
        return fbird_blob_open($trans, $blobId);
    }

    /**
     * Get the next generator/sequence value.
     *
     * @param string $generator Generator name
     * @param int $increment Increment value (default 1)
     * @return int|string Next value (string for large values)
     * @throws \Exception If generation fails
     */
    public function genId(string $generator, int $increment = 1): int|string
    {
        $result = fbird_gen_id($generator, $increment, $this->resource);
        if ($result === false) {
            throw new \Exception(fbird_errmsg() ?: "Failed to get generator value for: {$generator}");
        }
        return $result;
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
