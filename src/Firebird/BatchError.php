<?php

/**
 * php-firebird: Batch Error Value Object
 *
 * Provides a type-safe wrapper for per-row batch execution errors.
 *
 * @package   Firebird
 * @author    Michael Wegener <mw@satware.com>
 * @copyright satware AG 2025
 * @license   PHP License (same as PHP itself)
 */

declare(strict_types=1);

namespace Firebird;

use InvalidArgumentException;
use Stringable;

/**
 * Immutable value object representing a per-row batch execution error.
 *
 * When batch execution encounters errors (constraint violations, data type
 * mismatches, etc.), each error is captured with its row position, SQLSTATE
 * code, and descriptive message.
 *
 * SQLSTATE classes (first 2 characters):
 * - 00: Success
 * - 01: Warning
 * - 02: No data
 * - 22: Data exception (overflow, division by zero, etc.)
 * - 23: Integrity constraint violation (PK, FK, unique, check)
 * - 42: Syntax error or access rule violation
 *
 * Usage:
 * ```php
 * // Create from array (from fbird_batch_execute())
 * $error = BatchError::fromArray([
 *     'position' => 5,
 *     'sqlstate' => '23000',
 *     'message' => 'violation of PRIMARY or UNIQUE KEY constraint'
 * ]);
 *
 * // Check error type
 * if ($error->isConstraintViolation()) {
 *     echo "Duplicate key at row {$error->position}";
 * }
 *
 * // String representation
 * echo $error; // "Row 5: [23000] violation of PRIMARY..."
 * ```
 *
 * @see https://github.com/satwareAG/php-firebird
 */
final class BatchError implements Stringable
{
    /**
     * SQLSTATE class constants for common error categories
     */
    public const SQLSTATE_SUCCESS = '00';
    public const SQLSTATE_WARNING = '01';
    public const SQLSTATE_NO_DATA = '02';
    public const SQLSTATE_DATA_EXCEPTION = '22';
    public const SQLSTATE_INTEGRITY_CONSTRAINT = '23';
    public const SQLSTATE_SYNTAX_ERROR = '42';

    /**
     * Row position where the error occurred (0-based).
     */
    public readonly int $position;

    /**
     * SQLSTATE code (5 characters, e.g., "23000").
     */
    public readonly string $sqlstate;

    /**
     * Human-readable error message.
     */
    public readonly string $message;

    /**
     * Private constructor - use factory methods.
     *
     * @param int    $position Row position (0-based)
     * @param string $sqlstate SQLSTATE code (5 chars)
     * @param string $message  Error message
     */
    private function __construct(int $position, string $sqlstate, string $message)
    {
        $this->position = $position;
        $this->sqlstate = $sqlstate;
        $this->message = $message;
    }

    /**
     * Create a BatchError from an array.
     *
     * Expected array keys:
     * - 'position': int - Row position (0-based)
     * - 'sqlstate': string - SQLSTATE code
     * - 'message': string - Error message
     *
     * @param array{position: int, sqlstate: string, message: string} $data Error data array
     * @return self
     * @throws InvalidArgumentException If required keys are missing
     */
    public static function fromArray(array $data): self
    {
        if (!isset($data['position'])) {
            throw new InvalidArgumentException('Missing required key: position');
        }
        if (!isset($data['sqlstate'])) {
            throw new InvalidArgumentException('Missing required key: sqlstate');
        }
        if (!isset($data['message'])) {
            throw new InvalidArgumentException('Missing required key: message');
        }

        return new self(
            (int) $data['position'],
            (string) $data['sqlstate'],
            (string) $data['message']
        );
    }

    /**
     * Get the SQLSTATE class (first 2 characters).
     *
     * The class indicates the general category of the error:
     * - "22": Data exception
     * - "23": Integrity constraint violation
     * - "42": Syntax error
     *
     * @return string Two-character SQLSTATE class
     */
    public function getErrorClass(): string
    {
        return substr($this->sqlstate, 0, 2);
    }

    /**
     * Check if this is an integrity constraint violation (class 23).
     *
     * Constraint violations include:
     * - PRIMARY KEY violations
     * - UNIQUE constraint violations
     * - FOREIGN KEY violations
     * - CHECK constraint violations
     * - NOT NULL violations
     *
     * @return bool True if constraint violation
     */
    public function isConstraintViolation(): bool
    {
        return $this->getErrorClass() === self::SQLSTATE_INTEGRITY_CONSTRAINT;
    }

    /**
     * Check if this is a data exception (class 22).
     *
     * Data exceptions include:
     * - Division by zero
     * - Numeric overflow
     * - Invalid character value
     * - String truncation
     *
     * @return bool True if data exception
     */
    public function isDataException(): bool
    {
        return $this->getErrorClass() === self::SQLSTATE_DATA_EXCEPTION;
    }

    /**
     * Check if this is a syntax error (class 42).
     *
     * @return bool True if syntax error
     */
    public function isSyntaxError(): bool
    {
        return $this->getErrorClass() === self::SQLSTATE_SYNTAX_ERROR;
    }

    /**
     * Check if this is a warning (class 01).
     *
     * @return bool True if warning
     */
    public function isWarning(): bool
    {
        return $this->getErrorClass() === self::SQLSTATE_WARNING;
    }

    /**
     * Get string representation of the error.
     *
     * Format: "Row {position}: [{sqlstate}] {message}"
     *
     * @return string Human-readable error string
     */
    public function __toString(): string
    {
        return sprintf(
            'Row %d: [%s] %s',
            $this->position,
            $this->sqlstate,
            $this->message
        );
    }

    /**
     * Debug information.
     *
     * @return array{position: int, sqlstate: string, message: string, class: string}
     */
    public function __debugInfo(): array
    {
        return [
            'position' => $this->position,
            'sqlstate' => $this->sqlstate,
            'message' => $this->message,
            'class' => $this->getErrorClass(),
        ];
    }
}
