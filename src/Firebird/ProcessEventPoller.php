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
 * Process-based event poller with timeout support.
 *
 * Uses process isolation via proc_open() to handle blocking event waits.
 * The parent process monitors the child with stream timeouts, allowing
 * reliable timeout handling even though isc_wait_for_event() is not
 * interruptible by signals.
 *
 * This is the most reliable strategy as it provides complete isolation
 * from the Firebird client library's signal handling.
 *
 * @see EventPollerInterface
 * @see EventPoller::create()
 */
class ProcessEventPoller implements EventPollerInterface
{
    /**
     * Minimum timeout granularity in milliseconds.
     * Process spawn overhead makes sub-10ms timeouts impractical.
     */
    private const MIN_TIMEOUT_MS = 10;

    /**
     * The event handler resource.
     * @var resource|null
     */
    private mixed $event;

    /**
     * Database connection string (extracted from event resource context).
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
     * Whether the poller has been freed.
     */
    private bool $freed = false;

    /**
     * Create a new process-based event poller.
     *
     * Note: This poller needs database connection details to spawn child processes
     * that can independently connect and wait for events. You must call
     * setConnectionDetails() before polling.
     *
     * @param resource $event The event handler resource from fbird_set_event_handler()
     */
    public function __construct(mixed $event)
    {
        if (!is_resource($event) && !($event instanceof \Firebird\Event)) {
            throw new \InvalidArgumentException(
                'Expected event handler resource from fbird_set_event_handler()'
            );
        }
        $this->event = $event;
    }

    /**
     * Set database connection details for child processes.
     *
     * Required before calling poll() as child processes need to establish
     * their own database connections.
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
     */
    public function poll(int $timeoutMs = -1): mixed
    {
        if ($this->freed) {
            throw new \RuntimeException('Event poller has been freed');
        }

        // If connection details not set, fall back to direct polling (no timeout)
        if ($this->database === null) {
            return $this->pollDirect($timeoutMs);
        }

        return $this->pollWithProcess($timeoutMs);
    }

    /**
     * Poll using direct fbird_poll_event() call (no timeout support).
     *
     * @param int $timeoutMs Timeout (ignored - direct call blocks indefinitely)
     *
     * @return mixed Event result
     */
    private function pollDirect(int $timeoutMs): mixed
    {
        // Use the C extension's poll function directly
        // Note: timeout_ms parameter in fbird_poll_event doesn't actually work
        // due to Firebird client limitations, but we pass it anyway
        return fbird_poll_event($this->event, $timeoutMs);
    }

    /**
     * Poll using process isolation for reliable timeout.
     *
     * @param int $timeoutMs Timeout in milliseconds
     *
     * @return mixed Event result or FBIRD_EVENT_TIMEOUT
     */
    private function pollWithProcess(int $timeoutMs): mixed
    {
        // Build the child script
        $script = $this->buildChildScript();
        $scriptFile = tempnam(sys_get_temp_dir(), 'fb_poll_') . '.php';

        if (file_put_contents($scriptFile, $script) === false) {
            throw new \RuntimeException('Failed to create temporary polling script');
        }

        try {
            return $this->executeChildProcess($scriptFile, $timeoutMs);
        } finally {
            @unlink($scriptFile);
        }
    }

    /**
     * Build PHP script for child process.
     *
     * @return string PHP script content
     */
    private function buildChildScript(): string
    {
        $events = array_map(fn($e) => addslashes($e), $this->eventNames);
        $eventList = "'" . implode("', '", $events) . "'";

        return <<<PHP
<?php
/**
 * Child process for event polling.
 * Auto-generated by ProcessEventPoller - do not edit.
 */

// Load extension
if (!extension_loaded('firebird')) {
    if (!dl('firebird.so')) {
        echo json_encode(['error' => 'Failed to load firebird extension']);
        exit(1);
    }
}

// Connection parameters from environment
\$database = getenv('FB_DATABASE');
\$username = getenv('FB_USERNAME');
\$password = getenv('FB_PASSWORD');

if (!\$database || !\$username) {
    echo json_encode(['error' => 'Missing database connection parameters']);
    exit(1);
}

// Connect to database
\$conn = @fbird_connect(\$database, \$username, \$password);
if (!\$conn) {
    echo json_encode(['error' => 'Database connection failed: ' . fbird_errmsg()]);
    exit(1);
}

// Set up event handler
\$eventFired = null;
\$eventCount = 0;

\$event = @fbird_set_event_handler(\$conn, function(\$name, \$count) use (&\$eventFired, &\$eventCount) {
    \$eventFired = \$name;
    \$eventCount = \$count;
    return false; // Stop after first event
}, {$eventList});

if (!\$event) {
    echo json_encode(['error' => 'Failed to set event handler: ' . fbird_errmsg()]);
    fbird_close(\$conn);
    exit(1);
}

// Poll for event (blocks until event fires)
\$result = @fbird_poll_event(\$event);

// Output result as JSON
if (\$eventFired !== null) {
    echo json_encode([
        'event' => \$eventFired,
        'count' => \$eventCount,
        'result' => \$result
    ]);
} elseif (\$result === false) {
    echo json_encode(['error' => 'Poll failed: ' . fbird_errmsg()]);
} else {
    echo json_encode(['result' => \$result]);
}

// Cleanup
@fbird_free_event_handler(\$event);
@fbird_close(\$conn);
PHP;
    }

    /**
     * Execute child process and monitor with timeout.
     *
     * @param string $scriptFile Path to PHP script
     * @param int    $timeoutMs  Timeout in milliseconds
     *
     * @return mixed Event result or FBIRD_EVENT_TIMEOUT
     */
    private function executeChildProcess(string $scriptFile, int $timeoutMs): mixed
    {
        $descriptors = [
            0 => ['pipe', 'r'],  // stdin
            1 => ['pipe', 'w'],  // stdout
            2 => ['pipe', 'w'],  // stderr
        ];

        $env = [
            'FB_DATABASE' => $this->database,
            'FB_USERNAME' => $this->username,
            'FB_PASSWORD' => $this->password,
        ];

        // Preserve PATH and other necessary environment variables
        foreach (['PATH', 'LD_LIBRARY_PATH', 'HOME', 'USER'] as $key) {
            if (($value = getenv($key)) !== false) {
                $env[$key] = $value;
            }
        }

        $process = proc_open(
            [PHP_BINARY, '-d', 'extension=firebird.so', $scriptFile],
            $descriptors,
            $pipes,
            null,
            $env
        );

        if (!is_resource($process)) {
            throw new \RuntimeException('Failed to spawn child process for event polling');
        }

        // Close stdin - child doesn't need it
        fclose($pipes[0]);

        // Set non-blocking mode on stdout
        stream_set_blocking($pipes[1], false);

        $output = '';
        $startTime = microtime(true);
        $timeoutSec = $timeoutMs > 0 ? $timeoutMs / 1000.0 : null;

        try {
            while (true) {
                // Check for output
                $chunk = fread($pipes[1], 8192);
                if ($chunk !== false && $chunk !== '') {
                    $output .= $chunk;
                    // If we got a complete JSON response, we're done
                    if (strpos($output, '}') !== false) {
                        break;
                    }
                }

                // Check process status
                $status = proc_get_status($process);
                if (!$status['running']) {
                    // Process ended - read any remaining output
                    while (($chunk = fread($pipes[1], 8192)) !== false && $chunk !== '') {
                        $output .= $chunk;
                    }
                    break;
                }

                // Check timeout
                if ($timeoutSec !== null) {
                    $elapsed = microtime(true) - $startTime;
                    if ($elapsed >= $timeoutSec) {
                        // Timeout - terminate child
                        proc_terminate($process, SIGTERM);
                        usleep(50000); // 50ms grace period
                        proc_terminate($process, SIGKILL);
                        return FBIRD_EVENT_TIMEOUT;
                    }
                }

                // Small sleep to avoid busy-waiting
                usleep(1000); // 1ms
            }
        } finally {
            fclose($pipes[1]);
            fclose($pipes[2]);
            proc_close($process);
        }

        // Parse result
        $data = json_decode(trim($output), true);

        if ($data === null) {
            throw new \RuntimeException('Invalid response from child process: ' . $output);
        }

        if (isset($data['error'])) {
            throw new \RuntimeException('Child process error: ' . $data['error']);
        }

        // Execute callback if event fired
        if (isset($data['event']) && $this->callback !== null) {
            $continue = ($this->callback)($data['event'], $data['count'] ?? 1);
            if ($continue === false) {
                return null; // Handler cancelled
            }
        }

        return $data['event'] ?? $data['result'] ?? null;
    }

    /**
     * {@inheritdoc}
     */
    public static function isAvailable(): bool
    {
        // Check if proc_open is available
        if (!function_exists('proc_open')) {
            return false;
        }

        // Check if proc_open is not disabled
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
        return 'process';
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
