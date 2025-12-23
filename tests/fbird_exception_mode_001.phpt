--TEST--
fbird_set_exception_mode() - Runtime exception mode switching
--EXTENSIONS--
firebird
--FILE--
<?php
// Test 1: Verify constants exist
echo "Constants defined:\n";
var_dump(defined('FBIRD_EXCEPTION_MODE_SILENT'));
var_dump(defined('FBIRD_EXCEPTION_MODE_THROW'));
var_dump(FBIRD_EXCEPTION_MODE_SILENT === 0);
var_dump(FBIRD_EXCEPTION_MODE_THROW === 1);

// Test 2: Verify function exists
echo "\nFunction exists:\n";
var_dump(function_exists('fbird_set_exception_mode'));
var_dump(function_exists('fbird_get_exception_mode'));

// Test 3: Default mode is SILENT (0)
echo "\nDefault mode:\n";
var_dump(fbird_get_exception_mode() === FBIRD_EXCEPTION_MODE_SILENT);

// Test 4: Set THROW mode and verify
echo "\nSet THROW mode:\n";
$result = fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_THROW);
var_dump($result === true);
var_dump(fbird_get_exception_mode() === FBIRD_EXCEPTION_MODE_THROW);

// Test 5: Invalid mode returns false
echo "\nInvalid mode:\n";
$result = @fbird_set_exception_mode(999);
var_dump($result === false);

// Test 6: Reset to SILENT
echo "\nReset to SILENT:\n";
$result = fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_SILENT);
var_dump($result === true);
var_dump(fbird_get_exception_mode() === FBIRD_EXCEPTION_MODE_SILENT);

echo "\nDone\n";
?>
--EXPECT--
Constants defined:
bool(true)
bool(true)
bool(true)
bool(true)

Function exists:
bool(true)
bool(true)

Default mode:
bool(true)

Set THROW mode:
bool(true)
bool(true)

Invalid mode:
bool(true)

Reset to SILENT:
bool(true)
bool(true)

Done
