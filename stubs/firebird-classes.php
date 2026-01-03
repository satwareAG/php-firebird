<?php

/**
 * PHP Firebird Extension Class Stubs
 *
 * This file provides stub declarations for classes provided by the php-firebird
 * C extension. These are used by static analysis tools and IDEs.
 *
 * Note: Userland classes (Database, Transaction, Batch, etc.) are distributed
 * with the main php-firebird package, not in this stubs package.
 *
 * @package   php-firebird-stubs
 * @version   7.0.0
 * @author    satware AG <info@satware.com>
 * @copyright 2025 satware AG
 * @license   PHP-3.01 https://www.php.net/license/3_01.txt
 * @link      https://github.com/satwareAG/php-firebird
 *
 * @since 7.0.0
 */

declare(strict_types=1);

namespace Firebird;

// Skip if the extension is already loaded (runtime)
if (extension_loaded('fbird')) {
    return;
}

/**
 * Opaque event handle class provided by the C extension.
 *
 * This class represents an event handler resource returned by
 * fbird_set_event_handler(). It is used internally and should
 * be passed to fbird_poll_event() or fbird_free_event_handler().
 *
 * @since 7.0.0
 */
final class Event
{
    /**
     * Private constructor - instances are created by the extension.
     */
    private function __construct()
    {
    }
}

/**
 * Exception class for Firebird errors when exception mode is enabled.
 *
 * When exception mode is enabled via fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_THROW),
 * all Firebird errors will throw this exception instead of returning false.
 *
 * @since 7.0.0
 */
class Exception extends \Exception
{
    /**
     * @var string The SQLSTATE error code
     */
    protected string $sqlState = '00000';

    /**
     * Get the SQLSTATE error code.
     *
     * Returns a 5-character SQLSTATE code according to SQL:2003 standard.
     * Common codes:
     * - "23000": Integrity constraint violation
     * - "42000": Syntax error or access rule violation
     * - "08000": Connection exception
     * - "HY000": General error
     *
     * @return string 5-character SQLSTATE code
     * @since 7.0.0
     */
    public function getSqlState(): string
    {
        return $this->sqlState;
    }
}
