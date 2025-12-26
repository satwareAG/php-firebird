<?php

/**
 * php-firebird: Rich Database Information Structure
 *
 * Provides a type-safe, structured interface for database metadata and statistics.
 *
 * @package   Firebird
 * @author    Michael Wegener <mw@satware.com>
 * @copyright satware AG 2025
 * @license   PHP License (same as PHP itself)
 */

declare(strict_types=1);

namespace Firebird;

/**
 * Immutable value object containing database information and statistics.
 *
 * This class provides structured access to database metadata retrieved via
 * isc_database_info() / IAttachment::getInfo(). Properties cover:
 * - I/O statistics (reads, writes, fetches, marks)
 * - Memory usage (current, max, allocation)
 * - Database structure (page size, ODS version, etc.)
 * - Connection info (attachment ID, server version)
 *
 * Usage:
 * ```php
 * // When fbird_connection_info() is implemented:
 * $info = DbInfo::fromConnection($db);
 *
 * echo "Page size: {$info->pageSize}\n";
 * echo "ODS version: {$info->odsVersion}.{$info->odsMinorVersion}\n";
 * echo "Current memory: {$info->currentMemory} bytes\n";
 * ```
 *
 * @see https://github.com/satwareAG/php-firebird
 * @see https://firebirdsql.org/file/documentation/reference_manuals/fblangref25-en/html/fblangref25-appx07-dbinfo.html
 */
final class DbInfo
{
    // I/O Statistics
    public readonly int $reads;
    public readonly int $writes;
    public readonly int $fetches;
    public readonly int $marks;

    // Memory Statistics
    public readonly int $currentMemory;
    public readonly int $maxMemory;

    // Database Structure
    public readonly int $pageSize;
    public readonly int $numBuffers;
    public readonly int $allocation;
    public readonly int $attachmentId;

    // ODS (On-Disk Structure) Version
    public readonly int $odsVersion;
    public readonly int $odsMinorVersion;

    // Database Settings
    public readonly int $sweepInterval;
    public readonly bool $noReserve;
    public readonly bool $forcedWrites;

    // Version Info
    public readonly string $iscVersion;
    public readonly string $firebirdVersion;

    // Connection Info
    public readonly int $connectionId;
    public readonly int $transactionCount;

    // Creation/Modification Info
    public readonly ?string $creationDate;

    // Database Path
    public readonly string $databaseName;

    // Active transactions/statements
    public readonly int $activeTransactionCount;

    /**
     * Private constructor - use factory methods.
     *
     * @param array<string, mixed> $data Database info array
     */
    private function __construct(array $data)
    {
        // I/O Statistics
        $this->reads = $data['reads'] ?? 0;
        $this->writes = $data['writes'] ?? 0;
        $this->fetches = $data['fetches'] ?? 0;
        $this->marks = $data['marks'] ?? 0;

        // Memory Statistics
        $this->currentMemory = $data['current_memory'] ?? 0;
        $this->maxMemory = $data['max_memory'] ?? 0;

        // Database Structure
        $this->pageSize = $data['page_size'] ?? 0;
        $this->numBuffers = $data['num_buffers'] ?? 0;
        $this->allocation = $data['allocation'] ?? 0;
        $this->attachmentId = $data['attachment_id'] ?? 0;

        // ODS Version
        $this->odsVersion = $data['ods_version'] ?? 0;
        $this->odsMinorVersion = $data['ods_minor_version'] ?? 0;

        // Database Settings
        $this->sweepInterval = $data['sweep_interval'] ?? 0;
        $this->noReserve = $data['no_reserve'] ?? false;
        $this->forcedWrites = $data['forced_writes'] ?? false;

        // Version Info
        $this->iscVersion = $data['isc_version'] ?? '';
        $this->firebirdVersion = $data['firebird_version'] ?? '';

        // Connection Info
        $this->connectionId = $data['connection_id'] ?? 0;
        $this->transactionCount = $data['transaction_count'] ?? 0;

        // Creation Info
        $this->creationDate = $data['creation_date'] ?? null;

        // Database Path
        $this->databaseName = $data['db_name'] ?? '';

        // Active transactions
        $this->activeTransactionCount = $data['active_transaction_count'] ?? 0;
    }

    /**
     * Create DbInfo from a database connection.
     *
     * @param mixed $connection Database connection resource
     * @return self
     */
    public static function fromConnection(mixed $connection): self
    {
        $data = fbird_connection_info($connection);

        return $data === false ? new self([]) : new self($data);
    }

    /**
     * Create DbInfo from an array of data.
     *
     * @param array<string, mixed> $data Database info array
     * @return self
     */
    public static function fromArray(array $data): self
    {
        return new self($data);
    }

    /**
     * Get the full ODS version string.
     *
     * @return string ODS version like "13.0" or "12.2"
     */
    public function getOdsVersionString(): string
    {
        return "{$this->odsVersion}.{$this->odsMinorVersion}";
    }

    /**
     * Get human-readable memory usage.
     *
     * @return string Formatted memory like "12.5 MB"
     */
    public function getCurrentMemoryFormatted(): string
    {
        return self::formatBytes($this->currentMemory);
    }

    /**
     * Get human-readable max memory usage.
     *
     * @return string Formatted memory like "64.0 MB"
     */
    public function getMaxMemoryFormatted(): string
    {
        return self::formatBytes($this->maxMemory);
    }

    /**
     * Get human-readable page size.
     *
     * @return string Formatted size like "16 KB"
     */
    public function getPageSizeFormatted(): string
    {
        return self::formatBytes($this->pageSize);
    }

    /**
     * Check if this is Firebird 3.0 or later (ODS 12+).
     *
     * @return bool
     */
    public function isFirebird3OrLater(): bool
    {
        return $this->odsVersion >= 12;
    }

    /**
     * Check if this is Firebird 4.0 or later (ODS 13+).
     *
     * @return bool
     */
    public function isFirebird4OrLater(): bool
    {
        return $this->odsVersion >= 13;
    }

    /**
     * Check if this is Firebird 5.0 or later (ODS 13.1+).
     *
     * @return bool
     */
    public function isFirebird5OrLater(): bool
    {
        return $this->odsVersion >= 13 && $this->odsMinorVersion >= 1;
    }

    /**
     * Get total I/O operations (reads + writes).
     *
     * @return int
     */
    public function getTotalIoOperations(): int
    {
        return $this->reads + $this->writes;
    }

    /**
     * Get read/write ratio.
     *
     * @return float Ratio of reads to writes (0 if no writes)
     */
    public function getReadWriteRatio(): float
    {
        if ($this->writes === 0) {
            return 0.0;
        }
        return $this->reads / $this->writes;
    }

    /**
     * Convert to array.
     *
     * @return array<string, mixed>
     */
    public function toArray(): array
    {
        return [
            'reads' => $this->reads,
            'writes' => $this->writes,
            'fetches' => $this->fetches,
            'marks' => $this->marks,
            'current_memory' => $this->currentMemory,
            'max_memory' => $this->maxMemory,
            'page_size' => $this->pageSize,
            'num_buffers' => $this->numBuffers,
            'allocation' => $this->allocation,
            'attachment_id' => $this->attachmentId,
            'ods_version' => $this->odsVersion,
            'ods_minor_version' => $this->odsMinorVersion,
            'sweep_interval' => $this->sweepInterval,
            'no_reserve' => $this->noReserve,
            'forced_writes' => $this->forcedWrites,
            'isc_version' => $this->iscVersion,
            'firebird_version' => $this->firebirdVersion,
            'connection_id' => $this->connectionId,
            'transaction_count' => $this->transactionCount,
            'creation_date' => $this->creationDate,
            'db_name' => $this->databaseName,
            'active_transaction_count' => $this->activeTransactionCount,
        ];
    }

    /**
     * Format bytes to human-readable string.
     *
     * @param int $bytes Number of bytes
     * @param int $precision Decimal places
     * @return string Formatted string like "12.5 MB"
     */
    private static function formatBytes(int $bytes, int $precision = 1): string
    {
        $units = ['B', 'KB', 'MB', 'GB', 'TB'];
        $bytes = max($bytes, 0);
        $pow = floor(($bytes ? log($bytes) : 0) / log(1024));
        $pow = min($pow, count($units) - 1);
        $bytes /= (1 << (10 * $pow));
        return round($bytes, $precision) . ' ' . $units[(int)$pow];
    }

    /**
     * Debug information.
     *
     * @return array<string, mixed>
     */
    public function __debugInfo(): array
    {
        return [
            'odsVersion' => $this->getOdsVersionString(),
            'pageSize' => $this->getPageSizeFormatted(),
            'currentMemory' => $this->getCurrentMemoryFormatted(),
            'maxMemory' => $this->getMaxMemoryFormatted(),
            'attachmentId' => $this->attachmentId,
            'firebirdVersion' => $this->firebirdVersion,
            'ioStats' => [
                'reads' => $this->reads,
                'writes' => $this->writes,
                'fetches' => $this->fetches,
                'marks' => $this->marks,
            ],
        ];
    }
}
