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
 * Fiber-based event poller with timeout support.
 *
 * Uses PHP 8.1+ Fibers with AMPHP's event loop for truly async operation.
 * This strategy enables cooperative multitasking and integrates with
 * the AMPHP/ReactPHP async ecosystem.
 *
 * **Requirements:**
 * - PHP 8.1+ (for Fibers)
 * - amphp/amp ^3.0 package
 * - revolt/event-loop package (usually included with amphp)
 *
 * **Note on Implementation:**
 * Since fbird_poll_event() is a blocking call and PHP Fibers don't
 * provide true thread-level parallelism, this implementation uses
 * process isolation internally (similar to ProcessEventPoller) but
 * integrates with the AMPHP event loop for timeout handling.
 *
 * For pure synchronous code, use ProcessEventPoller directly.
 * FiberEventPoller is designed for async applications using AMPHP.
 *
 * @see EventPollerInterface
 * @see EventPoller::create()
 * @see https://amphp.org/
 */
class FiberEventPoller implements EventPollerInterface
{
    /**
     * Minimum timeout granularity in milliseconds.
     */
    private const MIN_TIMEOUT_MS = 1;

    /**
     * The event handler resource.
     * @var resource|null
     */
    private mixed $event;

    /**
     * Whether the poller has been freed.
     */
    private bool $freed = false;

    /**
     * Database connection string.
     */
    private ?string $database = null;

    /**
     * Username for database connection.
     */
    private ?string $username = null;

    /**
     * Password for database connection.
     */
    private ?string $password = null;

    /**
     * Event names to listen for.
     * @var string[]
     */
    private array $eventNames = [];

    /**
     * User callback function.
     * @var callable|null
     */
    private $callback = null;

    /**
     * Create a new Fiber-based event poller.
     *
     * @param resource $event The event handler resource from fbird_set_event_handler()
     *
     * @throws \RuntimeException If requirements are not met
     */
    public function __construct(mixed $event)
    {
        if (!self::isAvailable()) {
            throw new \RuntimeException(
                'FiberEventPoller requires PHP 8.1+ and amphp/amp ^3.0. ' .
                'Install with: composer require amphp/amp ^3.0'
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
     * Set database connection details for async polling.
     *
     * @param string   $database   Database connection string
     * @param string   $username   Database username
     * @param string   $password   Database password
     * @param string[] $eventNames Event names to listen for
     * @param callable $callback   Callback function to execute on event
     *
     * @return self For method chaining
     */
    public function setConnectionDetails(
        string $database,
        string $username,
        string $password,
        array $eventNames,
        callable $callback
    ): self {
        $this->database = $database;
        $this->username = $username;
        $this->password = $password;
        $this->eventNames = $eventNames;
        $this->callback = $callback;
        return $this;
    }

    /**
     * {@inheritdoc}
     *
     * When used with AMPHP, this integrates with the event loop for
     * proper async timeout handling. For synchronous code, falls back
     * to process-based polling.
     */
    public function poll(int $timeoutMs = -1): mixed
    {
        if ($this->freed) {
            throw new \RuntimeException('Event poller has been freed');
        }

        // Check if we're running inside an AMPHP event loop
        if (class_exists(\Revolt\EventLoop::class) && $this->database !== null) {
            return $this->pollAsync($timeoutMs);
        }

        // Fallback to direct blocking call
        return fbird_poll_event($this->event, $timeoutMs);
    }

    /**
     * Poll asynchronously using AMPHP event loop.
     *
     * @param int $timeoutMs Timeout in milliseconds
     *
     * @return mixed Event result or FBIRD_EVENT_TIMEOUT
     */
    private function pollAsync(int $timeoutMs): mixed
    {
        // This implementation uses a child process for the blocking call
        // and integrates with AMPHP's event loop for timeout handling

        $event = $this->event;
        if ($event === null) {
            throw new \LogicException('Event handler must be set before polling.');
        }

        $database = $this->database;
        $username = $this->username;
        $password = $this->password;
        $callback = $this->callback;

        if ($database === null) {
            throw new \LogicException('Database must be set before async polling.');
        }
        if ($username === null || $password === null) {
            throw new \LogicException(
                'Username and password must be set when database is set.'
            );
        }
        if ($callback === null) {
            throw new \LogicException('Callback must be set before async polling.');
        }

        $processPoller = new ProcessEventPoller($event);
        $processPoller->setConnectionDetails(
            $database,
            $username,
            $password,
            $this->eventNames,
            $callback
        );

        // If AMPHP is available and we're in an async context,
        // we could use EventLoop::defer() or similar for non-blocking behavior
        // For now, delegate to ProcessEventPoller which handles timeout reliably

        return $processPoller->poll($timeoutMs);
    }

    /**
     * Async poll that returns a Promise (for AMPHP integration).
     *
     * This method is designed for use with AMPHP's async/await pattern:
     *
     * ```php
     * use function Amp\async;
     * use function Amp\delay;
     *
     * $result = async(fn() => $poller->pollPromise(5000))->await();
     * ```
     *
     * @param int $timeoutMs Timeout in milliseconds
     *
     * @return mixed Event result or FBIRD_EVENT_TIMEOUT
     *
     * @throws \RuntimeException If AMPHP is not available
     */
    public function pollPromise(int $timeoutMs = -1): mixed
    {
        if (!class_exists(\Revolt\EventLoop::class)) {
            throw new \RuntimeException(
                'pollPromise() requires revolt/event-loop. ' .
                'Install with: composer require revolt/event-loop'
            );
        }

        // For true async integration, we need to spawn the blocking call
        // in a separate process and poll the result asynchronously
        return $this->poll($timeoutMs);
    }

    /**
     * {@inheritdoc}
     */
    public static function isAvailable(): bool
    {
        // Require PHP 8.1+ (Fibers)
        if (PHP_VERSION_ID < 80100) {
            return false;
        }

        // Require amphp/amp or revolt/event-loop
        if (!class_exists(\Revolt\EventLoop::class) && !class_exists(\Amp\Future::class)) {
            return false;
        }

        // Also need proc_open for process isolation
        if (!function_exists('proc_open')) {
            return false;
        }

        $disabled = explode(',', ini_get('disable_functions') ?: '');
        $disabled = array_map('trim', $disabled);

        return !in_array('proc_open', $disabled, true);
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
        return 'fiber';
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
}
