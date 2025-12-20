<?php

declare(strict_types=1);

/**
 * php-firebird: OO Transaction Wrapper
 *
 * Provides an object-oriented interface for transaction management.
 *
 * @package   Firebird
 * @author    Michael Wegener <mw@satware.com>
 * @copyright satware AG 2025
 * @license   PHP License (same as PHP itself)
 */

namespace Firebird;

/**
 * Object-oriented wrapper for Firebird transactions.
 *
 * This class provides a modern OO interface for transaction management,
 * wrapping the procedural fbird_trans(), fbird_commit(), fbird_rollback()
 * functions.
 *
 * Usage:
 * ```php
 * use Firebird\Database;
 *
 * $db = Database::connect('localhost:/path/to/db.fdb', 'SYSDBA', 'masterkey');
 *
 * // Method 1: Using TBuilder through Database
 * $trans = $db->transaction()
 *     ->readCommitted()
 *     ->wait()
 *     ->start();
 *
 * // Method 2: Direct creation
 * $trans = Transaction::begin($db);
 *
 * try {
 *     $db->queryWithTransaction($trans, "INSERT INTO ...");
 *     $trans->commit();
 * } catch (\Exception $e) {
 *     $trans->rollback();
 *     throw $e;
 * }
 *
 * // Method 3: Using savepoints
 * $trans->savepoint('sp1');
 * // ... do work ...
 * $trans->rollbackToSavepoint('sp1');  // Undo to savepoint
 * $trans->releaseSavepoint('sp1');     // Release savepoint
 * ```
 *
 * @see https://github.com/satwareAG/php-firebird
 */
class Transaction
{
    private mixed $resource;
    private mixed $connection;
    private bool $committed = false;
    private bool $rolledBack = false;

    /** @var array<string, bool> */
    private array $savepoints = [];

    /**
     * Private constructor - use factory methods.
     */
    private function __construct(mixed $resource, mixed $connection)
    {
        $this->resource = $resource;
        $this->connection = $connection;
    }

    /**
     * Begin a new transaction with default parameters.
     *
     * @param Database|mixed $connection Database instance or connection resource
     * @return self
     * @throws \Exception If transaction start fails
     */
    public static function begin(mixed $connection): self
    {
        $conn = $connection instanceof Database ? $connection->getResource() : $connection;
        $resource = fbird_trans_begin($conn, FBIRD_DEFAULT);

        if ($resource === false) {
            throw new \Exception(fbird_errmsg() ?: 'Failed to start transaction');
        }

        return new self($resource, $conn);
    }

    /**
     * Create a transaction from an existing resource.
     *
     * @param mixed $resource Transaction resource
     * @param mixed $connection Connection resource
     * @return self
     */
    public static function fromResource(mixed $resource, mixed $connection): self
    {
        return new self($resource, $connection);
    }

    /**
     * Get the underlying transaction resource.
     *
     * @return mixed
     */
    public function getResource(): mixed
    {
        return $this->resource;
    }

    /**
     * Get the Firebird transaction ID.
     *
     * Returns the internal Firebird transaction identifier, useful for
     * debugging and logging purposes.
     *
     * @return int|null Transaction ID, or null if transaction is not active
     */
    public function getId(): ?int
    {
        if (!$this->isActive()) {
            return null;
        }

        $info = fbird_trans_info($this->resource);
        return $info['id'] ?? null;
    }

    /**
     * Get detailed transaction information.
     *
     * Returns an array with transaction details:
     * - id: Transaction ID
     * - isolation: Isolation level (CONCURRENCY, READ_COMMITTED, CONSISTENCY)
     * - lock_timeout: Lock timeout in seconds
     * - access_mode: READ_ONLY or READ_WRITE
     * - state: Transaction state (ACTIVE)
     *
     * @return array<string, mixed>|null Transaction info, or null if not active
     */
    public function getInfo(): ?array
    {
        if (!$this->isActive()) {
            return null;
        }

        $info = fbird_trans_info($this->resource);
        return $info !== false ? $info : null;
    }

    /**
     * Check if transaction is still active.
     *
     * @return bool
     */
    public function isActive(): bool
    {
        return !$this->committed && !$this->rolledBack && $this->resource !== null;
    }

    /**
     * Check if transaction was committed.
     *
     * @return bool
     */
    public function isCommitted(): bool
    {
        return $this->committed;
    }

    /**
     * Check if transaction was rolled back.
     *
     * @return bool
     */
    public function isRolledBack(): bool
    {
        return $this->rolledBack;
    }

    /**
     * Commit the transaction.
     *
     * @return bool
     * @throws \Exception If commit fails
     */
    public function commit(): bool
    {
        if (!$this->isActive()) {
            throw new \Exception('Transaction is not active');
        }

        $result = fbird_commit($this->resource);
        if ($result === false) {
            throw new \Exception(fbird_errmsg() ?: 'Failed to commit transaction');
        }

        $this->committed = true;
        $this->resource = null;
        return true;
    }

    /**
     * Commit the transaction and retain it for reuse.
     *
     * The transaction remains active after commit, allowing further operations.
     *
     * @return bool
     * @throws \Exception If commit fails
     */
    public function commitRetaining(): bool
    {
        if (!$this->isActive()) {
            throw new \Exception('Transaction is not active');
        }

        $result = fbird_commit_ret($this->resource);
        if ($result === false) {
            throw new \Exception(fbird_errmsg() ?: 'Failed to commit transaction with retain');
        }

        return true;
    }

    /**
     * Rollback the transaction.
     *
     * @return bool
     * @throws \Exception If rollback fails
     */
    public function rollback(): bool
    {
        if (!$this->isActive()) {
            throw new \Exception('Transaction is not active');
        }

        $result = fbird_rollback($this->resource);
        if ($result === false) {
            throw new \Exception(fbird_errmsg() ?: 'Failed to rollback transaction');
        }

        $this->rolledBack = true;
        $this->resource = null;
        return true;
    }

    /**
     * Rollback the transaction and retain it for reuse.
     *
     * The transaction remains active after rollback, allowing further operations.
     *
     * @return bool
     * @throws \Exception If rollback fails
     */
    public function rollbackRetaining(): bool
    {
        if (!$this->isActive()) {
            throw new \Exception('Transaction is not active');
        }

        $result = fbird_rollback_ret($this->resource);
        if ($result === false) {
            throw new \Exception(fbird_errmsg() ?: 'Failed to rollback transaction with retain');
        }

        return true;
    }

    /**
     * Create a savepoint.
     *
     * @param string $name Savepoint name
     * @return self For method chaining
     * @throws \Exception If savepoint creation fails
     */
    public function savepoint(string $name): self
    {
        if (!$this->isActive()) {
            throw new \Exception('Transaction is not active');
        }

        // Validate savepoint name (alphanumeric + underscore only)
        if (!preg_match('/^[a-zA-Z_][a-zA-Z0-9_]*$/', $name)) {
            throw new \Exception(
                'Invalid savepoint name: must start with letter/underscore, contain only alphanumeric/underscore'
            );
        }

        $result = fbird_query_params_tx($this->connection, $this->resource, "SAVEPOINT {$name}");
        if ($result === false) {
            throw new \Exception(fbird_errmsg() ?: "Failed to create savepoint: {$name}");
        }

        $this->savepoints[$name] = true;
        return $this;
    }

    /**
     * Rollback to a savepoint.
     *
     * @param string $name Savepoint name
     * @return self For method chaining
     * @throws \Exception If rollback to savepoint fails
     */
    public function rollbackToSavepoint(string $name): self
    {
        if (!$this->isActive()) {
            throw new \Exception('Transaction is not active');
        }

        if (!isset($this->savepoints[$name])) {
            throw new \Exception("Savepoint not found: {$name}");
        }

        $result = fbird_query_params_tx($this->connection, $this->resource, "ROLLBACK TO SAVEPOINT {$name}");
        if ($result === false) {
            throw new \Exception(fbird_errmsg() ?: "Failed to rollback to savepoint: {$name}");
        }

        return $this;
    }

    /**
     * Release a savepoint.
     *
     * @param string $name Savepoint name
     * @return self For method chaining
     * @throws \Exception If release fails
     */
    public function releaseSavepoint(string $name): self
    {
        if (!$this->isActive()) {
            throw new \Exception('Transaction is not active');
        }

        if (!isset($this->savepoints[$name])) {
            throw new \Exception("Savepoint not found: {$name}");
        }

        $result = fbird_query_params_tx($this->connection, $this->resource, "RELEASE SAVEPOINT {$name}");
        if ($result === false) {
            throw new \Exception(fbird_errmsg() ?: "Failed to release savepoint: {$name}");
        }

        unset($this->savepoints[$name]);
        return $this;
    }

    /**
     * Get list of active savepoints.
     *
     * @return array<string>
     */
    public function getSavepoints(): array
    {
        return array_keys($this->savepoints);
    }

    /**
     * Execute a query within this transaction.
     *
     * @param string $sql SQL query
     * @param array<int, mixed> $params Parameters
     * @return mixed Query result
     */
    public function query(string $sql, array $params = []): mixed
    {
        if (!$this->isActive()) {
            throw new \Exception('Transaction is not active');
        }

        return fbird_query_params_tx($this->connection, $this->resource, $sql, $params);
    }

    /**
     * Destructor - rolls back uncommitted transaction.
     */
    public function __destruct()
    {
        if ($this->isActive()) {
            @fbird_rollback($this->resource);
        }
    }
}
