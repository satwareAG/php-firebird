<?php

/**
 * php-firebird: Batch Result Container
 *
 * Provides a type-safe container for batch execution results.
 *
 * @package   Firebird
 * @author    Michael Wegener <mw@satware.com>
 * @copyright satware AG 2025
 * @license   PHP License (same as PHP itself)
 */

declare(strict_types=1);

namespace Firebird;

use ArrayIterator;
use Countable;
use InvalidArgumentException;
use IteratorAggregate;
use Traversable;

/**
 * Immutable container for batch execution results.
 *
 * After executing a batch operation with IBatch, this class provides
 * structured access to the execution results including success/error
 * counts and detailed per-row error information.
 *
 * Implements Countable (count of errors) and IteratorAggregate (iterate errors)
 * for convenient error handling.
 *
 * Usage:
 * ```php
 * // From batch execution
 * $result = BatchResult::fromArray(fbird_batch_execute($batch));
 *
 * // Check overall status
 * if ($result->hasErrors()) {
 *     echo "Success rate: " . ($result->getSuccessRate() * 100) . "%\n";
 *
 *     // Iterate through errors
 *     foreach ($result as $error) {
 *         echo "Error at row {$error->position}: {$error->message}\n";
 *     }
 * }
 *
 * // Get summary
 * echo $result->getSummary();
 * ```
 *
 * @see https://github.com/satwareAG/php-firebird
 * @implements IteratorAggregate<int, BatchError>
 */
final class BatchResult implements Countable, IteratorAggregate
{
    /**
     * Total number of rows processed by the batch.
     */
    public readonly int $totalRows;

    /**
     * Number of rows successfully processed.
     */
    public readonly int $successCount;

    /**
     * Number of rows that failed.
     */
    public readonly int $errorCount;

    /**
     * Array of BatchError objects for failed rows.
     *
     * @var BatchError[]
     */
    private array $errors = [];

    /**
     * Private constructor - use factory methods.
     *
     * @param int          $totalRows    Total rows processed
     * @param int          $successCount Successful rows
     * @param int          $errorCount   Failed rows
     * @param BatchError[] $errors       Error details
     */
    private function __construct(
        int $totalRows,
        int $successCount,
        int $errorCount,
        array $errors = []
    ) {
        $this->totalRows = $totalRows;
        $this->successCount = $successCount;
        $this->errorCount = $errorCount;
        $this->errors = $errors;
    }

    /**
     * Create a BatchResult from an array.
     *
     * Expected array format (from fbird_batch_execute()):
     * - 'total_processed': int - Total rows processed
     * - 'success_count': int - Successful rows
     * - 'error_count': int - Failed rows
     * - 'errors': array (optional) - Per-row error details
     *
     * @param array{
     *     total_processed: int,
     *     success_count: int,
     *     error_count: int,
     *     errors?: array<array{position: int, sqlstate: string, message: string}>
     * } $data Result data array
     * @return self
     * @throws InvalidArgumentException If required keys are missing
     */
    public static function fromArray(array $data): self
    {
        if (!isset($data['total_processed'])) {
            throw new InvalidArgumentException('Missing required key: total_processed');
        }
        if (!isset($data['success_count'])) {
            throw new InvalidArgumentException('Missing required key: success_count');
        }
        if (!isset($data['error_count'])) {
            throw new InvalidArgumentException('Missing required key: error_count');
        }

        $errors = [];
        if (isset($data['errors']) && is_array($data['errors'])) {
            foreach ($data['errors'] as $errorData) {
                $errors[] = BatchError::fromArray($errorData);
            }
        }

        return new self(
            (int) $data['total_processed'],
            (int) $data['success_count'],
            (int) $data['error_count'],
            $errors
        );
    }

    /**
     * Create an empty result (no rows processed).
     *
     * @return self
     */
    public static function empty(): self
    {
        return new self(0, 0, 0);
    }

    /**
     * Check if the batch had any errors.
     *
     * @return bool True if at least one row failed
     */
    public function hasErrors(): bool
    {
        return $this->errorCount > 0;
    }

    /**
     * Check if all rows were processed successfully.
     *
     * @return bool True if no errors occurred
     */
    public function isComplete(): bool
    {
        return $this->errorCount === 0 && $this->totalRows > 0;
    }

    /**
     * Get the success rate as a float (0.0 to 1.0).
     *
     * @return float Success rate (e.g., 0.8 for 80%)
     */
    public function getSuccessRate(): float
    {
        if ($this->totalRows === 0) {
            return 0.0;
        }
        return $this->successCount / $this->totalRows;
    }

    /**
     * Get all errors as an array.
     *
     * @return BatchError[] Array of BatchError objects
     */
    public function getErrors(): array
    {
        return $this->errors;
    }

    /**
     * Get error at a specific row position.
     *
     * @param int $position Row position (0-based)
     * @return BatchError|null Error at position or null if none
     */
    public function getErrorAt(int $position): ?BatchError
    {
        foreach ($this->errors as $error) {
            if ($error->position === $position) {
                return $error;
            }
        }
        return null;
    }

    /**
     * Get the first error (if any).
     *
     * @return BatchError|null First error or null if no errors
     */
    public function getFirstError(): ?BatchError
    {
        return $this->errors[0] ?? null;
    }

    /**
     * Get the number of errors (Countable interface).
     *
     * @return int Number of errors
     */
    public function count(): int
    {
        return count($this->errors);
    }

    /**
     * Get iterator over errors (IteratorAggregate interface).
     *
     * @return Traversable<int, BatchError>
     */
    public function getIterator(): Traversable
    {
        return new ArrayIterator($this->errors);
    }

    /**
     * Get a human-readable summary of the batch result.
     *
     * @return string Summary string
     */
    public function getSummary(): string
    {
        $rate = $this->getSuccessRate() * 100;

        if ($this->totalRows === 0) {
            return 'Batch empty: no rows processed';
        }

        if ($this->isComplete()) {
            return sprintf(
                'Batch complete: %d/%d rows succeeded (100%%)',
                $this->successCount,
                $this->totalRows
            );
        }

        return sprintf(
            'Batch complete: %d/%d succeeded (%.1f%%), %d errors',
            $this->successCount,
            $this->totalRows,
            $rate,
            $this->errorCount
        );
    }

    /**
     * Debug information.
     *
     * @return array{totalRows: int, successCount: int, errorCount: int, successRate: float, hasErrors: bool}
     */
    public function __debugInfo(): array
    {
        return [
            'totalRows' => $this->totalRows,
            'successCount' => $this->successCount,
            'errorCount' => $this->errorCount,
            'successRate' => $this->getSuccessRate(),
            'hasErrors' => $this->hasErrors(),
        ];
    }
}
