<?php

/**
 * php-firebird: Batch Operations OO Wrapper
 *
 * Provides an object-oriented interface for Firebird IBatch bulk operations.
 *
 * @package   Firebird
 * @author    Michael Wegener <mw@satware.com>
 * @copyright satware AG 2025
 * @license   PHP License (same as PHP itself)
 */

declare(strict_types=1);

namespace Firebird;

use RuntimeException;

/**
 * Object-oriented wrapper for Firebird IBatch bulk operations.
 *
 * This class provides a fluent interface around the procedural \fbird_batch_*
 * functions for efficient bulk insert operations. IBatch (Firebird 4.0+)
 * offers significant performance improvements for inserting large numbers
 * of rows compared to individual INSERT statements.
 *
 * Key features:
 * - Fluent interface for adding rows
 * - Automatic resource management
 * - Type-safe BatchResult return from execute()
 * - BLOB support via addBlob() method
 *
 * Usage:
 * ```php
 * // Create batch from prepared statement
 * $stmt = \fbird_prepare($db, "INSERT INTO users (id, name) VALUES (?, ?)");
 * $batch = Batch::fromQuery($stmt);
 *
 * // Add rows using fluent interface
 * $batch->add(1, 'Alice')
 *       ->add(2, 'Bob')
 *       ->add(3, 'Charlie');
 *
 * // Execute and get result
 * $result = $batch->execute();
 *
 * if ($result->hasErrors()) {
 *     foreach ($result as $error) {
 *         echo "Error at row {$error->position}\n";
 *     }
 * }
 *
 * echo $result->getSummary();
 * ```
 *
 * @see https://github.com/satwareAG/php-firebird
 */
final class Batch
{
    /**
     * The underlying batch resource from \fbird_batch_create().
     *
     * @var resource|null
     */
    private mixed $resource;

    /**
     * Number of rows added to the batch.
     */
    private int $rowCount = 0;

    /**
     * Whether the batch has been executed.
     */
    private bool $executed = false;

    /**
     * BLOB IDs created within this batch context.
     *
     * @var BlobId[]
     */
    private array $blobIds = [];

    /**
     * Private constructor - use factory methods.
     *
     * @param resource $resource Batch resource
     */
    private function __construct(mixed $resource)
    {
        $this->resource = $resource;
    }

    /**
     * Create a Batch from a prepared query resource.
     *
     * @param resource $query Prepared statement from \fbird_prepare()
     * @return self
     * @throws RuntimeException If batch creation fails
     */
    public static function fromQuery(mixed $query): self
    {
        if (!function_exists('\fbird_batch_create')) {
            throw new RuntimeException(
                'IBatch API requires Firebird 4.0+ and php-firebird compiled with FB_API_VER >= 40'
            );
        }

        $resource = \fbird_batch_create($query);
        if ($resource === false) {
            throw new RuntimeException(
                'Failed to create batch: ' . (\fbird_errmsg() ?: 'Unknown error')
            );
        }

        return new self($resource);
    }

    /**
     * Create a Batch from an existing batch resource.
     *
     * Useful when working with batch resources created elsewhere.
     *
     * @param resource $resource Existing batch resource
     * @return self
     * @throws RuntimeException If resource is invalid
     */
    public static function fromResource(mixed $resource): self
    {
        // M3: Accept both legacy resources and Firebird\BatchHandle objects
        if (!is_resource($resource) && !($resource instanceof \Firebird\BatchHandle)) {
            throw new RuntimeException('Invalid batch resource');
        }

        return new self($resource);
    }

    /**
     * Add a row of parameters to the batch.
     *
     * Parameters should match the prepared statement's placeholders.
     * Returns $this for fluent interface.
     *
     * @param mixed ...$params Parameter values for the row
     * @return self For fluent interface
     * @throws RuntimeException If batch has been executed or add fails
     */
    public function add(mixed ...$params): self
    {
        $this->ensureNotExecuted();
        $this->ensureValidResource();
        assert($this->isValidBatchHandle());

        $result = \fbird_batch_add($this->resource, ...$params);
        if ($result === false) {
            throw new RuntimeException(
                'Failed to add row to batch: ' . (\fbird_errmsg() ?: 'Unknown error')
            );
        }

        $this->rowCount++;
        return $this;
    }

    /**
     * Execute the batch and return results.
     *
     * After execution, no more rows can be added to this batch.
     *
     * @return BatchResult Execution result with success/error counts
     * @throws RuntimeException If execution fails
     */
    public function execute(): BatchResult
    {
        $this->ensureNotExecuted();
        $this->ensureValidResource();
        assert($this->isValidBatchHandle());

        $result = \fbird_batch_execute($this->resource);
        if ($result === false) {
            throw new RuntimeException(
                'Failed to execute batch: ' . (\fbird_errmsg() ?: 'Unknown error')
            );
        }

        $this->executed = true;

        return BatchResult::fromArray($result);
    }

    /**
     * Cancel the batch operation without executing.
     *
     * Releases resources and marks the batch as unusable.
     *
     * @return void
     */
    public function cancel(): void
    {
        if ($this->isValidBatchHandle()) {
            \fbird_batch_cancel($this->resource);
        }
        $this->resource = null;
        $this->executed = true;
        $this->rowCount = 0;
    }

    /**
     * Create an inline BLOB within this batch context.
     *
     * Returns a BlobId that can be used as a parameter in add().
     *
     * @param string $data   BLOB data
     * @param int    $type   BLOB subtype (0 = binary, 1 = text)
     * @return BlobId BLOB identifier for use in add()
     * @throws RuntimeException If BLOB creation fails
     */
    public function addBlob(string $data, int $type = 0): BlobId
    {
        $this->ensureNotExecuted();
        $this->ensureValidResource();
        assert($this->isValidBatchHandle());

        $blobIdStr = \fbird_batch_add_blob($this->resource, $data, $type);
        if ($blobIdStr === false) {
            throw new RuntimeException(
                'Failed to add BLOB to batch: ' . (\fbird_errmsg() ?: 'Unknown error')
            );
        }

        $blobId = BlobId::fromString($blobIdStr);
        $this->blobIds[] = $blobId;

        return $blobId;
    }

    /**
     * Register an existing BLOB for use in this batch.
     *
     * Converts the existing BLOB to a batch-compatible format.
     *
     * @param BlobId $existingBlob Existing BLOB ID
     * @return BlobId Batch-compatible BLOB ID
     * @throws RuntimeException If registration fails
     */
    public function registerBlob(BlobId $existingBlob): BlobId
    {
        $this->ensureNotExecuted();
        $this->ensureValidResource();
        assert($this->isValidBatchHandle());

        $blobIdStr = \fbird_batch_register_blob($this->resource, (string) $existingBlob);
        if ($blobIdStr === false) {
            throw new RuntimeException(
                'Failed to register BLOB in batch: ' . (\fbird_errmsg() ?: 'Unknown error')
            );
        }

        $blobId = BlobId::fromString($blobIdStr);
        $this->blobIds[] = $blobId;

        return $blobId;
    }

    /**
     * Get the BLOB alignment requirement for this batch.
     *
     * @return int Alignment in bytes
     * @throws RuntimeException If query fails
     */
    public function getBlobAlignment(): int
    {
        $this->ensureValidResource();
        assert($this->isValidBatchHandle());

        $alignment = \fbird_batch_get_blob_alignment($this->resource);
        if ($alignment === false) {
            throw new RuntimeException(
                'Failed to get BLOB alignment: ' . (\fbird_errmsg() ?: 'Unknown error')
            );
        }

        return $alignment;
    }

    /**
     * Get the number of rows added to the batch.
     *
     * @return int Row count
     */
    public function getRowCount(): int
    {
        return $this->rowCount;
    }

    /**
     * Get the underlying batch resource.
     *
     * @return resource|null The batch resource or null if cancelled
     */
    public function getResource(): mixed
    {
        return $this->resource;
    }

    /**
     * Check if the batch has been executed.
     *
     * @return bool True if executed or cancelled
     */
    public function isExecuted(): bool
    {
        return $this->executed;
    }

    /**
     * Get BLOB IDs created within this batch.
     *
     * @return BlobId[] Array of BlobId objects
     */
    public function getBlobIds(): array
    {
        return $this->blobIds;
    }

    /**
     * Ensure the batch hasn't been executed yet.
     *
     * @throws RuntimeException If already executed
     */
    private function ensureNotExecuted(): void
    {
        if ($this->executed) {
            throw new RuntimeException('Batch has already been executed or cancelled');
        }
    }

    /**
     * Ensure the resource is still valid.
     *
     * @throws RuntimeException If resource is invalid
     */
    private function ensureValidResource(): void
    {
        if ($this->resource === null) {
            throw new RuntimeException('Batch resource is no longer valid');
        }
        // M3: fbird_batch_create() returns Firebird\BatchHandle object.
        // Accept both legacy resources and new objects.
        if (!is_resource($this->resource) && !($this->resource instanceof \Firebird\BatchHandle)) {
            throw new RuntimeException('Batch resource is no longer valid');
        }
    }

    /**
     * Check if the internal batch handle is valid (resource or BatchHandle object).
     */
    private function isValidBatchHandle(): bool
    {
        return $this->resource !== null
            && (is_resource($this->resource) || $this->resource instanceof \Firebird\BatchHandle);
    }

    /**
     * Debug information.
     *
     * @return array{rowCount: int, executed: bool, blobCount: int, hasResource: bool}
     */
    public function __debugInfo(): array
    {
        return [
            'rowCount' => $this->rowCount,
            'executed' => $this->executed,
            'blobCount' => count($this->blobIds),
            'hasResource' => $this->isValidBatchHandle(),
        ];
    }

    /**
     * Cleanup on destruction.
     *
     * Cancels the batch if not yet executed.
     */
    public function __destruct()
    {
        if (!$this->executed && $this->resource !== null) {
            $this->cancel();
        }
    }
}
