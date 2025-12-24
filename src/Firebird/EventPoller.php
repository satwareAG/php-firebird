<?php

/**
 * php-firebird: Event Timeout Wrapper Classes
 *
 * @package   Firebird
 * @author    Michael Wegener <mw@satware.com>
 * @copyright satware AG 2025
 * @license   PHP License (same as PHP itself)
 */

declare(strict_types=1);

namespace Firebird;

/**
 * Factory for creating event pollers with timeout support.
 *
 * Provides automatic strategy detection based on available PHP extensions
 * and runtime environment.
 *
 * Usage:
 * ```php
 * use Firebird\EventPoller;
 *
 * $conn = fbird_connect($database, $user, $password);
 * $event = fbird_set_event_handler($conn, function($name, $count) {
 *     echo "Event: $name ($count)\n";
 *     return true;
 * }, 'MY_EVENT');
 *
 * // Auto-detect best strategy
 * $poller = EventPoller::create($event);
 *
 * // Or specify strategy
 * $poller = EventPoller::create($event, 'process');
 *
 * // Poll with 5 second timeout
 * while (true) {
 *     $result = $poller->poll(5000);
 *     if ($result === FBIRD_EVENT_TIMEOUT) {
 *         echo "Timeout - doing background work...\n";
 *         continue;
 *     }
 *     if ($result === false) {
 *         break; // Error
 *     }
 *     echo "Event processed\n";
 * }
 *
 * $poller->free();
 * ```
 *
 * @see EventPollerInterface
 */
class EventPoller
{
    /**
     * Available strategies in order of preference.
     *
     * Process is most reliable as it provides complete isolation.
     * PCNTL may not work due to Firebird client signal handling.
     * Fiber requires external dependencies (amphp).
     */
    public const STRATEGIES = [
        'process' => ProcessEventPoller::class,
        'pcntl'   => PcntlEventPoller::class,
        'fiber'   => FiberEventPoller::class,
    ];

    /**
     * Create an event poller with the specified strategy.
     *
     * @param resource $event    The event handler resource from fbird_set_event_handler()
     * @param string   $strategy Strategy name: 'auto', 'process', 'pcntl', 'fiber'
     *
     * @return EventPollerInterface The configured poller instance
     *
     * @throws \InvalidArgumentException If strategy is unknown
     * @throws \RuntimeException If no suitable strategy is available
     */
    public static function create(mixed $event, string $strategy = 'auto'): EventPollerInterface
    {
        if ($strategy === 'auto') {
            return self::detectBestStrategy($event);
        }

        if (!isset(self::STRATEGIES[$strategy])) {
            throw new \InvalidArgumentException(sprintf(
                'Unknown event poller strategy: "%s". Available: %s',
                $strategy,
                implode(', ', array_keys(self::STRATEGIES))
            ));
        }

        $class = self::STRATEGIES[$strategy];

        if (!$class::isAvailable()) {
            throw new \RuntimeException(sprintf(
                'Event poller strategy "%s" is not available in this environment. %s',
                $strategy,
                self::getRequirements($strategy)
            ));
        }

        return new $class($event);
    }

    /**
     * Automatically detect and return the best available strategy.
     *
     * Strategy selection priority:
     * 1. ProcessEventPoller - Most reliable, works everywhere with proc_open
     * 2. PcntlEventPoller - Simple but may not work due to signal limitations
     * 3. FiberEventPoller - Requires amphp, best for async applications
     *
     * @param resource $event The event handler resource
     *
     * @return EventPollerInterface Best available poller
     *
     * @throws \RuntimeException If no strategy is available
     */
    public static function detectBestStrategy(mixed $event): EventPollerInterface
    {
        // Prefer process-based - most reliable
        if (ProcessEventPoller::isAvailable()) {
            return new ProcessEventPoller($event);
        }

        // PCNTL as fallback (may have signal limitations)
        if (PcntlEventPoller::isAvailable()) {
            return new PcntlEventPoller($event);
        }

        // Fiber-based requires external dependencies
        if (FiberEventPoller::isAvailable()) {
            return new FiberEventPoller($event);
        }

        throw new \RuntimeException(
            'No event poller strategy available. Requirements: ' .
            'proc_open() for process strategy, ' .
            'pcntl extension for signal strategy, ' .
            'or amphp/amp for fiber strategy.'
        );
    }

    /**
     * Get list of available strategies in current environment.
     *
     * @return array<string, array{available: bool, class: string, min_timeout_ms: int}>
     */
    public static function getAvailableStrategies(): array
    {
        $result = [];
        foreach (self::STRATEGIES as $name => $class) {
            $result[$name] = [
                'available' => $class::isAvailable(),
                'class' => $class,
                'min_timeout_ms' => $class::isAvailable() ? $class::getMinTimeoutMs() : 0,
            ];
        }
        return $result;
    }

    /**
     * Check if any polling strategy is available.
     *
     * @return bool True if at least one strategy can be used
     */
    public static function hasAvailableStrategy(): bool
    {
        foreach (self::STRATEGIES as $class) {
            if ($class::isAvailable()) {
                return true;
            }
        }
        return false;
    }

    /**
     * Get human-readable requirements for a strategy.
     *
     * @param string $strategy Strategy name
     *
     * @return string Requirements description
     */
    private static function getRequirements(string $strategy): string
    {
        return match ($strategy) {
            'process' => 'Requires proc_open() function (not disabled in PHP config).',
            'pcntl' => 'Requires pcntl extension. Note: May not work due to Firebird client signal handling.',
            'fiber' => 'Requires PHP 8.1+ and amphp/amp ^3.0 package.',
            default => 'Unknown strategy.',
        };
    }
}
