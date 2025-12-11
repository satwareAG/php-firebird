--TEST--
Event Poller Wrapper Classes - Basic Functionality
--EXTENSIONS--
firebird
--SKIPIF--
<?php
// Include firebird test helpers
include __DIR__ . '/skipif.inc';
?>
--FILE--
<?php
/**
 * Test the PHP Event Poller wrapper classes for timeout support.
 *
 * This test verifies:
 * 1. All wrapper classes can be loaded
 * 2. Factory detection works correctly
 * 3. Strategy availability checks work
 * 4. FBIRD_EVENT_TIMEOUT constant is defined
 */

// Ensure autoloading works
spl_autoload_register(function ($class) {
    $prefix = 'Firebird\\';
    $baseDir = __DIR__ . '/../src/Firebird/';

    if (strncmp($prefix, $class, strlen($prefix)) !== 0) {
        return;
    }

    $relativeClass = substr($class, strlen($prefix));
    $file = $baseDir . str_replace('\\', '/', $relativeClass) . '.php';

    if (file_exists($file)) {
        require $file;
    }
});

use Firebird\EventPoller;
use Firebird\EventPollerInterface;
use Firebird\ProcessEventPoller;
use Firebird\PcntlEventPoller;
use Firebird\FiberEventPoller;

echo "=== Event Poller Wrapper Tests ===\n\n";

// Test 1: Verify FBIRD_EVENT_TIMEOUT constant
echo "1. FBIRD_EVENT_TIMEOUT constant:\n";
if (defined('FBIRD_EVENT_TIMEOUT')) {
    echo "   Defined: Yes\n";
    echo "   Value: " . FBIRD_EVENT_TIMEOUT . "\n";
    echo "   PASS\n";
} else {
    echo "   Defined: No\n";
    echo "   FAIL - Constant not defined\n";
}
echo "\n";

// Test 2: Verify IBASE_EVENT_TIMEOUT alias (optional - may not be defined)
echo "2. IBASE_EVENT_TIMEOUT alias:\n";
if (defined('IBASE_EVENT_TIMEOUT')) {
    echo "   Defined: Yes\n";
    echo "   Value: " . IBASE_EVENT_TIMEOUT . "\n";
    echo "   Matches FBIRD: " . (IBASE_EVENT_TIMEOUT === FBIRD_EVENT_TIMEOUT ? "Yes" : "No") . "\n";
} else {
    echo "   Defined: No (optional alias)\n";
}
echo "   PASS\n";
echo "\n";

// Test 3: EventPollerInterface exists
echo "3. EventPollerInterface:\n";
if (interface_exists(EventPollerInterface::class)) {
    echo "   Interface exists: Yes\n";

    $methods = ['poll', 'isAvailable', 'getMinTimeoutMs', 'getStrategyName', 'free'];
    $reflection = new ReflectionClass(EventPollerInterface::class);
    $actualMethods = array_map(fn($m) => $m->getName(), $reflection->getMethods());

    echo "   Required methods:\n";
    foreach ($methods as $method) {
        $has = in_array($method, $actualMethods);
        echo "     - {$method}(): " . ($has ? "Yes" : "No") . "\n";
    }
    echo "   PASS\n";
} else {
    echo "   Interface exists: No\n";
    echo "   FAIL - Interface not found\n";
}
echo "\n";

// Test 4: EventPoller factory exists
echo "4. EventPoller factory:\n";
if (class_exists(EventPoller::class)) {
    echo "   Class exists: Yes\n";

    $strategies = EventPoller::STRATEGIES;
    echo "   Defined strategies:\n";
    foreach ($strategies as $name => $class) {
        echo "     - {$name}: {$class}\n";
    }
    echo "   PASS\n";
} else {
    echo "   Class exists: No\n";
    echo "   FAIL - Factory not found\n";
}
echo "\n";

// Test 5: ProcessEventPoller availability
echo "5. ProcessEventPoller:\n";
if (class_exists(ProcessEventPoller::class)) {
    echo "   Class exists: Yes\n";
    echo "   isAvailable(): " . (ProcessEventPoller::isAvailable() ? "Yes" : "No") . "\n";
    echo "   getMinTimeoutMs(): " . ProcessEventPoller::getMinTimeoutMs() . "ms\n";
    echo "   getStrategyName(): " . ProcessEventPoller::getStrategyName() . "\n";

    // Verify it implements interface
    $implements = class_implements(ProcessEventPoller::class);
    $implementsInterface = in_array(EventPollerInterface::class, $implements);
    echo "   Implements EventPollerInterface: " . ($implementsInterface ? "Yes" : "No") . "\n";
    echo "   PASS\n";
} else {
    echo "   Class exists: No\n";
    echo "   FAIL - Class not found\n";
}
echo "\n";

// Test 6: PcntlEventPoller availability
echo "6. PcntlEventPoller:\n";
if (class_exists(PcntlEventPoller::class)) {
    echo "   Class exists: Yes\n";
    echo "   isAvailable(): " . (PcntlEventPoller::isAvailable() ? "Yes" : "No") . "\n";
    echo "   getMinTimeoutMs(): " . PcntlEventPoller::getMinTimeoutMs() . "ms\n";
    echo "   getStrategyName(): " . PcntlEventPoller::getStrategyName() . "\n";

    $implements = class_implements(PcntlEventPoller::class);
    $implementsInterface = in_array(EventPollerInterface::class, $implements);
    echo "   Implements EventPollerInterface: " . ($implementsInterface ? "Yes" : "No") . "\n";
    echo "   PASS\n";
} else {
    echo "   Class exists: No\n";
    echo "   FAIL - Class not found\n";
}
echo "\n";

// Test 7: FiberEventPoller (may not be available)
echo "7. FiberEventPoller:\n";
if (class_exists(FiberEventPoller::class)) {
    echo "   Class exists: Yes\n";
    echo "   isAvailable(): " . (FiberEventPoller::isAvailable() ? "Yes" : "No (requires amphp)") . "\n";
    echo "   getMinTimeoutMs(): " . FiberEventPoller::getMinTimeoutMs() . "ms\n";
    echo "   getStrategyName(): " . FiberEventPoller::getStrategyName() . "\n";

    $implements = class_implements(FiberEventPoller::class);
    $implementsInterface = in_array(EventPollerInterface::class, $implements);
    echo "   Implements EventPollerInterface: " . ($implementsInterface ? "Yes" : "No") . "\n";
    echo "   PASS\n";
} else {
    echo "   Class exists: No\n";
    echo "   FAIL - Class not found\n";
}
echo "\n";

// Test 8: Factory strategy detection
echo "8. Factory strategy detection:\n";
$available = EventPoller::getAvailableStrategies();
echo "   Available strategies:\n";
foreach ($available as $name => $info) {
    $status = $info['available'] ? "Available" : "Not available";
    $minTimeout = $info['available'] ? " (min: {$info['min_timeout_ms']}ms)" : "";
    echo "     - {$name}: {$status}{$minTimeout}\n";
}

$hasAny = EventPoller::hasAvailableStrategy();
echo "   Has any available: " . ($hasAny ? "Yes" : "No") . "\n";
echo "   PASS\n";
echo "\n";

// Test 9: Factory error handling
echo "9. Factory error handling:\n";
try {
    EventPoller::create(null, 'invalid_strategy');
    echo "   FAIL - Should have thrown InvalidArgumentException\n";
} catch (InvalidArgumentException $e) {
    echo "   Invalid strategy correctly throws: InvalidArgumentException\n";
    echo "   Message: " . substr($e->getMessage(), 0, 50) . "...\n";
    echo "   PASS\n";
} catch (Throwable $e) {
    echo "   FAIL - Wrong exception type: " . get_class($e) . "\n";
}
echo "\n";

echo "=== All Tests Complete ===\n";
?>
--EXPECTF--
=== Event Poller Wrapper Tests ===

1. FBIRD_EVENT_TIMEOUT constant:
   Defined: Yes
   Value: -2
   PASS

2. IBASE_EVENT_TIMEOUT alias:
   Defined: %s
   PASS

3. EventPollerInterface:
   Interface exists: Yes
   Required methods:
     - poll(): Yes
     - isAvailable(): Yes
     - getMinTimeoutMs(): Yes
     - getStrategyName(): Yes
     - free(): Yes
   PASS

4. EventPoller factory:
   Class exists: Yes
   Defined strategies:
     - process: Firebird\ProcessEventPoller
     - pcntl: Firebird\PcntlEventPoller
     - fiber: Firebird\FiberEventPoller
   PASS

5. ProcessEventPoller:
   Class exists: Yes
   isAvailable(): Yes
   getMinTimeoutMs(): 10ms
   getStrategyName(): process
   Implements EventPollerInterface: Yes
   PASS

6. PcntlEventPoller:
   Class exists: Yes
   isAvailable(): Yes
   getMinTimeoutMs(): 1000ms
   getStrategyName(): pcntl
   Implements EventPollerInterface: Yes
   PASS

7. FiberEventPoller:
   Class exists: Yes
   isAvailable(): %s
   getMinTimeoutMs(): 1ms
   getStrategyName(): fiber
   Implements EventPollerInterface: Yes
   PASS

8. Factory strategy detection:
   Available strategies:
     - process: %s
     - pcntl: %s
     - fiber: %s
   Has any available: Yes
   PASS

9. Factory error handling:
   Invalid strategy correctly throws: InvalidArgumentException
   Message: Unknown event poller strategy: "invalid_strategy%s
   PASS

=== All Tests Complete ===
