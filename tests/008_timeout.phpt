--TEST--
Firebird: fbird_poll_event() timeout API and constant
--SKIPIF--
<?php
if (PHP_OS == "WINNT") die("skip: Timeout feature not supported on Windows");
include("skipif.inc");
?>
--FILE--
<?php
/**
 * Test fbird_poll_event() timeout API functionality.
 *
 * Tests:
 * - FBIRD_EVENT_TIMEOUT constant exists and has correct value (-2)
 * - fbird_poll_event() arginfo accepts optional timeout parameter
 *
 * NOTE: Full timeout behavior testing requires platform-specific signal
 * handling that may not work with isc_wait_for_event() on all platforms.
 */

require("firebird.inc");

echo "Test 1: FBIRD_EVENT_TIMEOUT constant\n";
if (defined('FBIRD_EVENT_TIMEOUT')) {
    $value = FBIRD_EVENT_TIMEOUT;
    echo "OK: FBIRD_EVENT_TIMEOUT = $value\n";
    if ($value === -2) {
        echo "OK: Correct value (-2)\n";
    } else {
        echo "FAIL: Expected -2, got $value\n";
    }
} else {
    echo "FAIL: FBIRD_EVENT_TIMEOUT constant not defined\n";
}

echo "\nTest 2: fbird_poll_event() function signature via reflection\n";
if (function_exists('fbird_poll_event')) {
    $rf = new ReflectionFunction('fbird_poll_event');
    $params = $rf->getParameters();

    echo "Parameters: " . count($params) . "\n";

    // Check first param (event resource)
    if (isset($params[0])) {
        echo "Param 1: " . $params[0]->getName() . " (required: " . ($params[0]->isOptional() ? "no" : "yes") . ")\n";
    }

    // Check second param (timeout_ms)
    if (isset($params[1])) {
        $p = $params[1];
        echo "Param 2: " . $p->getName();
        echo " (optional: " . ($p->isOptional() ? "yes" : "no") . ")";
        if ($p->isOptional() && $p->isDefaultValueAvailable()) {
            echo " (default: " . $p->getDefaultValue() . ")";
        }
        echo "\n";
        echo "OK: Timeout parameter exists\n";
    } else {
        echo "FAIL: Timeout parameter not found\n";
    }
} else {
    echo "FAIL: fbird_poll_event function not found\n";
}

echo "\nTest 3: Verify ibase_poll_event alias exists\n";
if (function_exists('ibase_poll_event')) {
    echo "OK: ibase_poll_event alias exists\n";
} else {
    echo "Note: ibase_poll_event alias not defined (fbird prefix only)\n";
}

echo "\nend of test\n";
?>
--EXPECT--
Test 1: FBIRD_EVENT_TIMEOUT constant
OK: FBIRD_EVENT_TIMEOUT = -2
OK: Correct value (-2)

Test 2: fbird_poll_event() function signature via reflection
Parameters: 2
Param 1: event (required: yes)
Param 2: timeout_ms (optional: yes)
OK: Timeout parameter exists

Test 3: Verify ibase_poll_event alias exists
Note: ibase_poll_event alias not defined (fbird prefix only)

end of test
