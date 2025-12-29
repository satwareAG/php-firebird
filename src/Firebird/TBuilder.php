<?php

declare(strict_types=1);

/**
 * php-firebird: Fluent Transaction Parameter Builder
 *
 * Provides a modern, fluent interface for building transaction parameters
 * that wrap the existing fbird_trans() constants.
 *
 * @package   Firebird
 * @author    Michael Wegener <mw@satware.com>
 * @copyright satware AG 2025
 * @license   PHP License (same as PHP itself)
 */

namespace Firebird;

/**
 * Fluent builder for Firebird Transaction Parameter Block (TPB) options.
 *
 * This class provides a type-safe, discoverable API for configuring transactions
 * instead of using raw bitmask constants. The build() method returns an array
 * compatible with fbird_trans() and fbird_trans_start().
 *
 * Usage:
 * ```php
 * $options = TBuilder::create()
 *     ->readOnly()
 *     ->isolationReadCommitted()
 *     ->recordVersion()
 *     ->wait(5)
 *     ->build();
 *
 * $trans = fbird_trans_start($db, $options);
 * ```
 *
 * @see https://github.com/satwareAG/php-firebird
 * @see https://firebirdsql.org/file/documentation/reference_manuals/fblangref25-en/html/fblangref25-transacs.html
 */
final class TBuilder
{
    // Connection resource for start() method
    private mixed $connection = null;

    // Access mode
    private ?int $accessMode = null;

    // Isolation level
    private ?int $isolation = null;

    // Record versioning (only with READ COMMITTED)
    private ?int $recordVersioning = null;

    // Lock resolution
    private ?int $lockResolution = null;

    // Lock timeout in seconds (only with WAIT)
    private ?int $lockTimeout = null;

    // Read consistency (Firebird 4.0+)
    private bool $readConsistency = false;

    /** @var array<string, int> Table reservations: ['table_name' => mode_flags, ...] */
    private array $tableReservations = [];

    /**
     * Create a new TBuilder instance.
     *
     * @return self
     */
    public static function create(): self
    {
        return new self();
    }

    /**
     * Private constructor - use create() factory method.
     */
    private function __construct()
    {
    }

    // =========================================================================
    // ACCESS MODE
    // =========================================================================

    /**
     * Set transaction to read-only mode.
     *
     * Read-only transactions cannot modify data but may have better
     * performance for reporting queries.
     *
     * @param bool $enable Enable read-only mode (default: true)
     * @return $this
     */
    public function readOnly(bool $enable = true): self
    {
        $this->accessMode = $enable ? FBIRD_READ : FBIRD_WRITE;
        return $this;
    }

    /**
     * Set transaction to read-write mode (default).
     *
     * @return $this
     */
    public function readWrite(): self
    {
        $this->accessMode = FBIRD_WRITE;
        return $this;
    }

    // =========================================================================
    // ISOLATION LEVELS
    // =========================================================================

    /**
     * Use SNAPSHOT isolation (also known as CONCURRENCY).
     *
     * Transaction sees a consistent snapshot of the database as of
     * transaction start. Other transactions' changes are not visible.
     *
     * This is the default isolation level in Firebird.
     *
     * @return $this
     */
    public function isolationSnapshot(): self
    {
        $this->isolation = FBIRD_CONCURRENCY;
        $this->recordVersioning = null;
        return $this;
    }

    /**
     * Alias for isolationSnapshot() - uses Firebird terminology.
     *
     * @return $this
     */
    public function isolationConcurrency(): self
    {
        return $this->isolationSnapshot();
    }

    /**
     * Use SNAPSHOT TABLE STABILITY isolation (also known as CONSISTENCY).
     *
     * Like SNAPSHOT but also prevents other transactions from modifying
     * tables that this transaction is reading. Provides serializable isolation.
     *
     * @return $this
     */
    public function isolationSnapshotTableStability(): self
    {
        $this->isolation = FBIRD_CONSISTENCY;
        $this->recordVersioning = null;
        return $this;
    }

    /**
     * Alias for isolationSnapshotTableStability() - uses internal terminology.
     *
     * @return $this
     */
    public function isolationConsistency(): self
    {
        return $this->isolationSnapshotTableStability();
    }

    /**
     * Use READ COMMITTED isolation with RECORD_VERSION.
     *
     * Transaction sees the latest committed version of each row.
     * With RECORD_VERSION: reads last committed version even if uncommitted change exists.
     *
     * @return $this
     */
    public function isolationReadCommittedRecordVersion(): self
    {
        $this->isolation = FBIRD_COMMITTED;
        $this->recordVersioning = FBIRD_REC_VERSION;
        return $this;
    }

    /**
     * Use READ COMMITTED isolation with NO RECORD_VERSION.
     *
     * Transaction sees the latest committed version of each row.
     * With NO RECORD_VERSION: waits for uncommitted changes to resolve.
     *
     * @return $this
     */
    public function isolationReadCommittedNoRecordVersion(): self
    {
        $this->isolation = FBIRD_COMMITTED;
        $this->recordVersioning = FBIRD_REC_NO_VERSION;
        return $this;
    }

    /**
     * Use READ COMMITTED isolation with READ CONSISTENCY (Firebird 4.0+).
     *
     * Provides statement-level snapshot consistency within READ COMMITTED.
     * Each statement sees a consistent view as of statement start.
     *
     * @return $this
     */
    public function isolationReadCommittedReadConsistency(): self
    {
        $this->isolation = FBIRD_COMMITTED;
        $this->readConsistency = true;
        $this->recordVersioning = null;
        return $this;
    }

    /**
     * Convenience method: Set READ COMMITTED isolation (defaults to RECORD_VERSION).
     *
     * @return $this
     */
    public function isolationReadCommitted(): self
    {
        return $this->isolationReadCommittedRecordVersion();
    }

    // =========================================================================
    // LOCK RESOLUTION
    // =========================================================================

    /**
     * Use WAIT lock resolution (default).
     *
     * Transaction waits for locks held by other transactions to be released.
     * Optionally specify a timeout.
     *
     * @param int $lockTimeout Timeout in seconds. Use -1 for infinite wait (default).
     *                         Use 0 or positive value for timeout.
     * @return $this
     */
    public function wait(int $lockTimeout = -1): self
    {
        $this->lockResolution = FBIRD_WAIT;
        $this->lockTimeout = ($lockTimeout >= 0) ? $lockTimeout : null;
        return $this;
    }

    /**
     * Use NO WAIT lock resolution.
     *
     * Transaction immediately fails with a conflict error if it cannot
     * acquire a lock due to another transaction.
     *
     * @return $this
     */
    public function noWait(): self
    {
        $this->lockResolution = FBIRD_NOWAIT;
        $this->lockTimeout = null;
        return $this;
    }

    // =========================================================================
    // ADVANCED OPTIONS
    // =========================================================================

    /**
     * Enable IGNORE LIMBO transactions.
     *
     * @param bool $enable Enable ignore limbo (default: true)
     * @return $this
     */
    public function ignoreLimbo(bool $enable = true): self
    {
        // Note: IGNORE_LIMBO is not exposed as a PHP constant in current implementation.
        // This is a placeholder for future implementation.
        // The TPB byte would be isc_tpb_ignore_limbo (value 8 in ibase.h)
        return $this;
    }

    /**
     * Enable AUTO COMMIT mode.
     *
     * Each statement is automatically committed after execution.
     * Not commonly used - explicit commit/rollback is preferred.
     *
     * @param bool $enable Enable auto commit (default: true)
     * @return $this
     */
    public function autoCommit(bool $enable = true): self
    {
        // Note: AUTO_COMMIT is not exposed as a PHP constant in current implementation.
        // This is a placeholder for future implementation.
        // The TPB byte would be isc_tpb_autocommit (value 16 in ibase.h)
        return $this;
    }

    /**
     * Enable NO AUTO UNDO mode.
     *
     * Disables automatic undo log maintenance. Use for large batch operations
     * where you know you won't need to rollback.
     *
     * @param bool $enable Enable no auto undo (default: true)
     * @return $this
     */
    public function noAutoUndo(bool $enable = true): self
    {
        // Note: NO_AUTO_UNDO is not exposed as a PHP constant in current implementation.
        // This is a placeholder for future implementation.
        // The TPB byte would be isc_tpb_no_auto_undo (value 24 in ibase.h)
        return $this;
    }

    // =========================================================================
    // TABLE RESERVATIONS
    // =========================================================================

    /**
     * Add a table reservation with SHARED READ lock.
     *
     * Allows other transactions to read and write the table.
     *
     * @param string $tableName Table name to reserve
     * @return $this
     */
    public function reserveSharedRead(string $tableName): self
    {
        $this->tableReservations[$tableName] = FBIRD_LOCK_READ | FBIRD_LOCK_SHARED;
        return $this;
    }

    /**
     * Add a table reservation with SHARED WRITE lock.
     *
     * Allows other transactions to read but not write the table.
     *
     * @param string $tableName Table name to reserve
     * @return $this
     */
    public function reserveSharedWrite(string $tableName): self
    {
        $this->tableReservations[$tableName] = FBIRD_LOCK_WRITE | FBIRD_LOCK_SHARED;
        return $this;
    }

    /**
     * Add a table reservation with PROTECTED READ lock.
     *
     * Prevents other SNAPSHOT TABLE STABILITY transactions from
     * locking the table.
     *
     * @param string $tableName Table name to reserve
     * @return $this
     */
    public function reserveProtectedRead(string $tableName): self
    {
        $this->tableReservations[$tableName] = FBIRD_LOCK_READ | FBIRD_LOCK_PROTECTED;
        return $this;
    }

    /**
     * Add a table reservation with PROTECTED WRITE lock.
     *
     * Prevents other transactions from writing to the table.
     *
     * @param string $tableName Table name to reserve
     * @return $this
     */
    public function reserveProtectedWrite(string $tableName): self
    {
        $this->tableReservations[$tableName] = FBIRD_LOCK_WRITE | FBIRD_LOCK_PROTECTED;
        return $this;
    }

    /**
     * Add a table reservation with EXCLUSIVE READ lock.
     *
     * Prevents all other transactions from accessing the table.
     *
     * @param string $tableName Table name to reserve
     * @return $this
     */
    public function reserveExclusiveRead(string $tableName): self
    {
        $this->tableReservations[$tableName] = FBIRD_LOCK_READ | FBIRD_LOCK_EXCLUSIVE;
        return $this;
    }

    /**
     * Add a table reservation with EXCLUSIVE WRITE lock.
     *
     * Prevents all other transactions from accessing the table.
     * Strongest lock level.
     *
     * @param string $tableName Table name to reserve
     * @return $this
     */
    public function reserveExclusiveWrite(string $tableName): self
    {
        $this->tableReservations[$tableName] = FBIRD_LOCK_WRITE | FBIRD_LOCK_EXCLUSIVE;
        return $this;
    }

    // =========================================================================
    // BUILD
    // =========================================================================

    /**
     * Build the options array for fbird_trans() / fbird_trans_start().
     *
     * Returns an associative array compatible with the new fbird_trans_start()
     * array options format. For legacy bitmask usage, use buildFlags() instead.
     *
     * @return array{
     *     access_mode?: int,
     *     isolation?: int,
     *     lock_resolution?: int,
     *     lock_timeout?: int,
     *     read_consistency?: bool,
     *     table_reservations?: array<string, int>
     * }
     */
    public function build(): array
    {
        $options = [];

        if ($this->accessMode !== null) {
            $options['access_mode'] = $this->accessMode;
        }

        if ($this->isolation !== null) {
            $options['isolation'] = $this->isolation;

            // Add record versioning if applicable
            if ($this->recordVersioning !== null) {
                $options['isolation'] |= $this->recordVersioning;
            }
        }

        if ($this->lockResolution !== null) {
            $options['lock_resolution'] = $this->lockResolution;
        }

        if ($this->lockTimeout !== null) {
            $options['lock_timeout'] = $this->lockTimeout;
        }

        if ($this->readConsistency) {
            $options['read_consistency'] = true;
        }

        if (!empty($this->tableReservations)) {
            $options['table_reservations'] = $this->tableReservations;
        }

        return $options;
    }

    /**
     * Build a bitmask flags value for legacy fbird_trans() usage.
     *
     * Use this when you need to pass a single integer value to the
     * traditional fbird_trans($db, $flags) call.
     *
     * @return int Combined bitmask of all configured options
     */
    public function buildFlags(): int
    {
        $flags = 0;

        if ($this->accessMode !== null) {
            $flags |= $this->accessMode;
        }

        if ($this->isolation !== null) {
            $flags |= $this->isolation;
        }

        if ($this->recordVersioning !== null) {
            $flags |= $this->recordVersioning;
        }

        if ($this->lockResolution !== null) {
            $flags |= $this->lockResolution;
        }

        if ($this->lockTimeout !== null) {
            $flags |= FBIRD_LOCK_TIMEOUT;
        }

        if ($this->readConsistency) {
            $flags |= FBIRD_READ_CONSISTENCY;
        }

        return $flags;
    }

    /**
     * Get the configured lock timeout value.
     *
     * @return int|null Lock timeout in seconds, or null if not set
     */
    public function getLockTimeout(): ?int
    {
        return $this->lockTimeout;
    }

    /**
     * Get the table reservations.
     *
     * @return array<string, int> Map of table name => lock mode flags
     */
    public function getTableReservations(): array
    {
        return $this->tableReservations;
    }

    /**
     * Reset builder to initial state.
     *
     * Note: Does NOT reset the connection - use resetAll() to reset everything.
     *
     * @return $this
     */
    public function reset(): self
    {
        $this->accessMode = null;
        $this->isolation = null;
        $this->recordVersioning = null;
        $this->lockResolution = null;
        $this->lockTimeout = null;
        $this->readConsistency = false;
        $this->tableReservations = [];
        return $this;
    }

    /**
     * Reset builder completely including connection.
     *
     * @return $this
     */
    public function resetAll(): self
    {
        $this->connection = null;
        return $this->reset();
    }

    /**
     * Create a copy of this builder for modification.
     *
     * @return self New builder instance with same configuration
     */
    public function copy(): self
    {
        $copy = new self();
        $copy->connection = $this->connection;
        $copy->accessMode = $this->accessMode;
        $copy->isolation = $this->isolation;
        $copy->recordVersioning = $this->recordVersioning;
        $copy->lockResolution = $this->lockResolution;
        $copy->lockTimeout = $this->lockTimeout;
        $copy->readConsistency = $this->readConsistency;
        $copy->tableReservations = $this->tableReservations;
        return $copy;
    }

    // =========================================================================
    // CONNECTION AND START
    // =========================================================================

    /**
     * Set the connection for this transaction builder.
     *
     * This enables the fluent start() method to create and return a Transaction.
     *
     * @param mixed $connection Database connection resource or Database instance
     * @return $this
     */
    public function connection(mixed $connection): self
    {
        if ($connection instanceof Database) {
            $this->connection = $connection->getResource();
        } else {
            $this->connection = $connection;
        }
        return $this;
    }

    /**
     * Start the transaction and return a Transaction object.
     *
     * This is a convenience method that builds the options and starts
     * the transaction in one step. Requires connection() to be called first.
     *
     * Usage:
     * ```php
     * $trans = TBuilder::create()
     *     ->connection($db)
     *     ->readCommitted()
     *     ->wait(5)
     *     ->start();
     * ```
     *
     * @return Transaction
     * @throws \Exception If connection not set or transaction start fails
     */
    public function start(): Transaction
    {
        if ($this->connection === null) {
            throw new \Exception('Connection not set. Call connection() before start().');
        }

        $options = $this->build();
        $resource = fbird_trans_start($this->connection, $options);

        if ($resource === false) {
            throw new \Exception(fbird_errmsg() ?: 'Failed to start transaction');
        }

        return Transaction::fromResource($resource, $this->connection);
    }

    /**
     * Check if a connection has been set.
     *
     * @return bool
     */
    public function hasConnection(): bool
    {
        return $this->connection !== null;
    }

    // =========================================================================
    // CONVENIENCE ALIASES (for documentation consistency)
    // =========================================================================

    /**
     * Alias for isolationReadCommitted() - shorter name.
     *
     * @return $this
     */
    public function readCommitted(): self
    {
        return $this->isolationReadCommitted();
    }

    /**
     * Alias for isolationSnapshot() - shorter name.
     *
     * @return $this
     */
    public function snapshot(): self
    {
        return $this->isolationSnapshot();
    }

    /**
     * Alias for isolationReadCommittedRecordVersion() - uses record version term.
     *
     * @return $this
     */
    public function recordVersion(): self
    {
        return $this->isolationReadCommittedRecordVersion();
    }
}
