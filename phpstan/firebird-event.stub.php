<?php

declare(strict_types=1);

namespace Firebird;

/**
 * Stub for extension-provided `Firebird\Event` class.
 *
 * The C extension may return either a resource or an instance of this class
 * from event-related APIs. This stub exists for static analysis only.
 */
final class Event
{
}

/**
 * Stub for extension-provided `Firebird\Exception` class.
 *
 * PDO-style exception for Firebird errors when exception mode is enabled.
 */
class Exception extends \Exception
{
    /**
     * Get SQLSTATE error code
     *
     * @return string 5-character SQLSTATE code (e.g., "42000", "HY000")
     */
    public function getSqlState(): string {}
}
