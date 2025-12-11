<?php

declare(strict_types=1);

/**
 * php-firebird: Event Timeout Wrapper Classes
 *
 * @package   Firebird
 * @author    Michael Wegener <mw@satware.com>
 * @copyright satware AG 2025
 * @license   PHP License (same as PHP itself)
 */

namespace Firebird;

/**
 * Interface for event polling strategies.
 *
 * Provides timeout support for fbird_poll_event() through various
 * implementation strategies (PCNTL signals, process isolation, Fibers).
 *
 * The Firebird client library's isc_wait_for_event() is NOT interruptible
 * by signals on Linux, so these PHP-level wrappers provide alternative
 * timeout mechanisms.
 *
 * @see https://github.com/satwareAG/php-firebird
 */
interface EventPollerInterface
{
    /**
     * Poll for events with timeout support.
     *
     * Blocks until an event fires or the timeout is reached.
     *
     * @param int $timeoutMs Timeout in milliseconds. Use -1 for indefinite blocking.
     *                       Note: Some strategies (PCNTL) only support second granularity.
     *
     * @return mixed Returns one of:
     *               - string: Event name that fired (callback was executed)
     *               - null: Handler was cancelled by callback returning false
     *               - false: An error occurred
     *               - FBIRD_EVENT_TIMEOUT (-2): Timeout reached without event
     *
     * @throws \RuntimeException If the polling strategy cannot be executed
     */
    public function poll(int $timeoutMs = -1): mixed;

    /**
     * Check if this polling strategy is available in the current environment.
     *
     * @return bool True if all requirements are met
     */
    public static function isAvailable(): bool;

    /**
     * Get the minimum timeout granularity in milliseconds.
     *
     * @return int Minimum timeout value that can be reliably used.
     *             For PCNTL this is 1000ms (1 second).
     *             For process-based this is typically 10ms+.
     */
    public static function getMinTimeoutMs(): int;

    /**
     * Get human-readable strategy name.
     *
     * @return string Strategy identifier (e.g., 'pcntl', 'process', 'fiber')
     */
    public static function getStrategyName(): string;

    /**
     * Free resources associated with this poller.
     *
     * Should be called when done polling to clean up the underlying
     * event handler resource.
     *
     * @return void
     */
    public function free(): void;
}
