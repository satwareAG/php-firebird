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
 * PCNTL signal-based event poller with timeout support.
 *
 * Uses pcntl_alarm() to send SIGALRM after the timeout period.
 * This attempts to interrupt the blocking isc_wait_for_event() call.
 *
 * **IMPORTANT LIMITATION:**
 * Due to the Firebird client library's internal signal handling,
 * SIGALRM may NOT reliably interrupt isc_wait_for_event() on Linux.
 * The Firebird client library likely:
 * - Restarts interrupted system calls automatically
 * - Uses signal-resistant blocking primitives
 * - Handles EINTR internally and retries
 *
 * This strategy is provided for completeness but may not work reliably.
 * Use ProcessEventPoller for guaranteed timeout support.
 *
 * @see EventPollerInterface
 * @see EventPoller::create()
 * @see ProcessEventPoller For reliable timeout support
 */
class PcntlEventPoller implements EventPollerInterface
{
    /**
     * Minimum timeout granularity in milliseconds.
     * pcntl_alarm() only supports second precision.
     */
    private const MIN_TIMEOUT_MS = 1000;

    /**
     * The event handler resource.
     * @var resource|null
     */
    private mixed $event;

    /**
     * Whether a timeout occurred.
     */
    private bool $timedOut = false;

    /**
     * Whether the poller has been freed.
     */
    private bool $freed = false;

    /**
     * Previous signal handler to restore after polling.
     * @var callable|int|null
     */
    private $previousHandler = null;

    /**
     * Create a new PCNTL-based event poller.
     *
     * @param resource $event The event handler resource from fbird_set_event_handler()
     *
     * @throws \RuntimeException If pcntl extension is not available
     */
    public function __construct(mixed $event)
    {
        if (!self::isAvailable()) {
            throw new \RuntimeException(
                'PCNTL extension is required for PcntlEventPoller. ' .
                'Use ProcessEventPoller as an alternative.'
            );
        }

        if (!is_resource($event) && !($event instanceof \Firebird\Event)) {
            throw new \InvalidArgumentException(
                'Expected event handler resource from fbird_set_event_handler()'
            );
        }

        $this->event = $event;
    }

    /**
     * {@inheritdoc}
     *
     * **Note:** Due to Firebird client library limitations, the timeout may not
     * actually interrupt the blocking call. This is a known limitation.
     * Use ProcessEventPoller for guaranteed timeout support.
     */
    public function poll(int $timeoutMs = -1): mixed
    {
        if ($this->freed) {
            throw new \RuntimeException('Event poller has been freed');
        }

        // No timeout requested - block indefinitely
        if ($timeoutMs < 0) {
            return fbird_poll_event($this->event);
        }

        // Convert to seconds (round up to ensure at least 1 second)
        $timeoutSeconds = (int) max(1, ceil($timeoutMs / 1000));

        return $this->pollWithAlarm($timeoutSeconds);
    }

    /**
     * Poll with SIGALRM timeout.
     *
     * @param int $timeoutSeconds Timeout in seconds
     *
     * @return mixed Event result or FBIRD_EVENT_TIMEOUT
     */
    private function pollWithAlarm(int $timeoutSeconds): mixed
    {
        $this->timedOut = false;

        // Install our signal handler
        $this->previousHandler = pcntl_signal(SIGALRM, function (int $signo): void {
            $this->timedOut = true;
        });

        // Enable async signals (PHP 7.1+)
        $asyncSignals = pcntl_async_signals(true);

        try {
            // Schedule alarm
            $previousAlarm = pcntl_alarm($timeoutSeconds);

            // Attempt blocking call
            $result = fbird_poll_event($this->event);

            // Cancel alarm
            pcntl_alarm(0);

            // Check if we timed out
            if ($this->timedOut) {
                return FBIRD_EVENT_TIMEOUT;
            }

            return $result;

        } finally {
            // Cancel any pending alarm
            pcntl_alarm(0);

            // Restore async signals setting
            pcntl_async_signals($asyncSignals);

            // Restore previous handler
            if ($this->previousHandler !== null) {
                pcntl_signal(SIGALRM, $this->previousHandler);
            } else {
                pcntl_signal(SIGALRM, SIG_DFL);
            }

            $this->previousHandler = null;
        }
    }

    /**
     * {@inheritdoc}
     */
    public static function isAvailable(): bool
    {
        // Check if pcntl extension is loaded
        if (!extension_loaded('pcntl')) {
            return false;
        }

        // Check if required functions exist
        $required = ['pcntl_alarm', 'pcntl_signal', 'pcntl_async_signals'];
        foreach ($required as $func) {
            if (!function_exists($func)) {
                return false;
            }
        }

        // Check if pcntl functions are not disabled
        $disabled = explode(',', ini_get('disable_functions') ?: '');
        $disabled = array_map('trim', $disabled);

        foreach ($required as $func) {
            if (in_array($func, $disabled, true)) {
                return false;
            }
        }

        return true;
    }

    /**
     * {@inheritdoc}
     */
    public static function getMinTimeoutMs(): int
    {
        return self::MIN_TIMEOUT_MS;
    }

    /**
     * {@inheritdoc}
     */
    public static function getStrategyName(): string
    {
        return 'pcntl';
    }

    /**
     * {@inheritdoc}
     */
    public function free(): void
    {
        if (!$this->freed && $this->event !== null) {
            @fbird_free_event_handler($this->event);
            $this->event = null;
            $this->freed = true;
        }
    }

    /**
     * Destructor - ensure cleanup.
     */
    public function __destruct()
    {
        $this->free();
    }

    /**
     * Check if the last poll operation timed out.
     *
     * Useful for debugging to confirm whether SIGALRM was received.
     *
     * @return bool True if the last poll timed out
     */
    public function didTimeout(): bool
    {
        return $this->timedOut;
    }
}
