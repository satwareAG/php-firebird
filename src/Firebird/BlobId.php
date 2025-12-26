<?php

/**
 * php-firebird: Type-safe BLOB Identifier Value Object
 *
 * Provides a type-safe wrapper around Firebird BLOB identifiers (ISC_QUAD).
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
 * Immutable value object representing a Firebird BLOB identifier.
 *
 * BLOB IDs in Firebird are 64-bit values (ISC_QUAD) represented as
 * hexadecimal strings in the format "0x" followed by 16 hex digits
 * (e.g., "0x0000000100000002").
 *
 * This class provides:
 * - Type safety: Distinguishes BLOB IDs from arbitrary strings
 * - Validation: Ensures proper BLOB ID format
 * - Immutability: BLOB IDs cannot be modified after creation
 * - Backward compatibility: __toString() allows seamless use with existing functions
 *
 * Usage:
 * ```php
 * // Create from string (from database)
 * $blobId = BlobId::fromString($row['blob_column']);
 *
 * // Use with existing functions (automatic string conversion)
 * $stream = fbird_blob_open($db, $blobId);
 *
 * // Compare BLOB IDs
 * if ($blobId->equals($otherBlobId)) {
 *     // Same BLOB
 * }
 *
 * // Check for NULL BLOB
 * if ($blobId->isNull()) {
 *     // BLOB is NULL
 * }
 * ```
 *
 * @see https://github.com/satwareAG/php-firebird
 */
final class BlobId implements Stringable
{
    /**
     * Expected length of BLOB ID string formats:
     * - Colon format: "HHHHHHHH:LLLL" = 13 chars (standard from php-firebird extension)
     * - Hex format: "0xHHHHHHHHHHHHHHHH" = 18 chars (legacy format)
     */
    public const ID_LENGTH_COLON = 13;
    public const ID_LENGTH_HEX = 18;

    /**
     * Regex patterns for valid BLOB ID formats
     */
    private const PATTERN_COLON = '/^[0-9a-fA-F]{8}:[0-9a-fA-F]{4}$/';
    private const PATTERN_HEX = '/^0x[0-9a-fA-F]{16}$/';

    /**
     * The raw BLOB ID string in hex format
     */
    private string $id;

    /**
     * High 32 bits of the BLOB ID (gds_quad_high)
     */
    private int $high;

    /**
     * Low 32 bits of the BLOB ID (gds_quad_low)
     */
    private int $low;

    /**
     * Private constructor - use factory methods.
     *
     * @param string $id The validated BLOB ID string
     * @param int $high High 32 bits
     * @param int $low Low 32 bits
     */
    private function __construct(string $id, int $high, int $low)
    {
        $this->id = strtolower($id);
        $this->high = $high;
        $this->low = $low;
    }

    /**
     * Create a BlobId from a string.
     *
     * Supports two formats:
     * - Colon format: "HHHHHHHH:LLLL" (13 chars, standard from php-firebird extension)
     * - Hex format: "0xHHHHHHHHHHHHHHHH" (18 chars, legacy)
     *
     * @param string $id BLOB ID string
     * @return self
     * @throws InvalidArgumentException If the string is not a valid BLOB ID
     */
    public static function fromString(string $id): self
    {
        $id = trim($id);

        if (!self::isValidFormat($id)) {
            throw new InvalidArgumentException(sprintf(
                'Invalid BLOB ID format: "%s". Expected formats: "HHHHHHHH:LLLL" or "0xHHHHHHHHHHHHHHHH".',
                strlen($id) > 30 ? substr($id, 0, 30) . '...' : $id
            ));
        }

        // Parse based on format detected
        if (str_contains($id, ':')) {
            // Colon format: "HHHHHHHH:LLLL"
            [$highHex, $lowHex] = explode(':', $id);
            $high = hexdec($highHex);
            $low = hexdec($lowHex);
            $normalized = sprintf('%08X:%04X', $high, $low);
        } else {
            // Hex format: "0xHHHHHHHHHHHHHHHH"
            // Colon format uses high 32 bits + middle 16 bits (not last 16)
            $hex = substr($id, 2); // Remove "0x" prefix
            $high = hexdec(substr($hex, 0, 8));
            // For colon format, use middle 16 bits (positions 8-11, not 12-15)
            $low = hexdec(substr($hex, 8, 4));
            // Normalize to colon format (standard)
            $normalized = sprintf('%08X:%04X', $high, $low);
        }

        return new self($normalized, (int)$high, (int)$low);
    }

    /**
     * Create a BlobId from high and low 32-bit parts.
     *
     * @param int $high High 32 bits (gds_quad_high)
     * @param int $low Low 32 bits (gds_quad_low - actually 16 bits used)
     * @return self
     */
    public static function fromParts(int $high, int $low): self
    {
        // Use colon format (standard)
        $id = sprintf('%08X:%04X', $high & 0xFFFFFFFF, $low & 0xFFFF);
        return new self($id, $high, $low);
    }

    /**
     * Create a NULL BLOB ID (all zeros).
     *
     * @return self
     */
    public static function null(): self
    {
        return new self('00000000:0000', 0, 0);
    }

    /**
     * Try to create a BlobId from a string, returning null on failure.
     *
     * @param string $id BLOB ID string
     * @return self|null BlobId instance or null if invalid format
     */
    public static function tryFromString(string $id): ?self
    {
        try {
            return self::fromString($id);
        } catch (InvalidArgumentException) {
            return null;
        }
    }

    /**
     * Check if a string is a valid BLOB ID format.
     *
     * Accepts both colon format ("HHHHHHHH:LLLL") and hex format ("0xHHHHHHHHHHHHHHHH").
     *
     * @param string $id String to check
     * @return bool True if valid format
     */
    public static function isValidFormat(string $id): bool
    {
        $len = strlen($id);

        if ($len === self::ID_LENGTH_COLON) {
            return preg_match(self::PATTERN_COLON, $id) === 1;
        }

        if ($len === self::ID_LENGTH_HEX) {
            return preg_match(self::PATTERN_HEX, $id) === 1;
        }

        return false;
    }

    /**
     * Get the string representation of the BLOB ID.
     *
     * Returns the hex string in lowercase format.
     *
     * @return string BLOB ID as hex string
     */
    public function __toString(): string
    {
        return $this->id;
    }

    /**
     * Get the BLOB ID as a string.
     *
     * @return string BLOB ID as hex string
     */
    public function toString(): string
    {
        return $this->id;
    }

    /**
     * Get the high 32 bits of the BLOB ID.
     *
     * Corresponds to ISC_QUAD.gds_quad_high in Firebird.
     *
     * @return int High 32 bits
     */
    public function getHigh(): int
    {
        return $this->high;
    }

    /**
     * Get the low 32 bits of the BLOB ID.
     *
     * Corresponds to ISC_QUAD.gds_quad_low in Firebird.
     *
     * @return int Low 32 bits
     */
    public function getLow(): int
    {
        return $this->low;
    }

    /**
     * Check if this BLOB ID is equal to another.
     *
     * @param self|string $other Another BlobId or string to compare
     * @return bool True if equal
     */
    public function equals(self|string $other): bool
    {
        if (is_string($other)) {
            $other = self::tryFromString($other);
            if ($other === null) {
                return false;
            }
        }

        return $this->high === $other->high && $this->low === $other->low;
    }

    /**
     * Check if this is a NULL BLOB (all zeros).
     *
     * A NULL BLOB ID has both high and low parts set to zero.
     *
     * @return bool True if this represents a NULL BLOB
     */
    public function isNull(): bool
    {
        return $this->high === 0 && $this->low === 0;
    }

    /**
     * Check if this BLOB ID is not NULL.
     *
     * @return bool True if this represents an actual BLOB
     */
    public function isNotNull(): bool
    {
        return !$this->isNull();
    }

    /**
     * Get the 64-bit integer representation (if platform supports it).
     *
     * Note: This may overflow on 32-bit platforms.
     *
     * @return int 64-bit representation
     */
    public function toInt64(): int
    {
        return ($this->high << 32) | ($this->low & 0xFFFFFFFF);
    }

    /**
     * Serialize to JSON as the string representation.
     *
     * @return string
     */
    public function jsonSerialize(): string
    {
        return $this->id;
    }

    /**
     * Debug information.
     *
     * @return array{id: string, high: int, low: int, isNull: bool}
     */
    public function __debugInfo(): array
    {
        return [
            'id' => $this->id,
            'high' => $this->high,
            'low' => $this->low,
            'isNull' => $this->isNull(),
        ];
    }
}
