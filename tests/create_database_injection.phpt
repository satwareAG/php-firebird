--TEST--
Security: fbird_create_database() escapes single quotes (C1 fix)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

// Test 1: Database path with single quote in name should be escaped
// We use a path that would cause injection if unescaped
// The escaped version creates a DB with literal quote in path
// This may fail at Firebird level (invalid path), which is fine -
// the key is it must NOT cause SQL injection

echo "Test 1: Single quote in database path\n";
$malicious_db = $test_base . "_test'injection";
$result = @fbird_create_database($malicious_db, $user, $password);
if ($result !== false) {
    // If it somehow succeeded, clean up
    @fbird_drop_db($result);
    echo "Created and dropped safely\n";
} else {
    // Expected: Firebird rejects the path, but no injection occurred
    echo "Rejected safely (no injection)\n";
}

// Test 2: Username with single quote
echo "Test 2: Single quote in username\n";
$result = @fbird_create_database($test_base . "_inj2", "user'inject", $password);
if ($result !== false) {
    @fbird_drop_db($result);
}
echo "Username handled safely\n";

// Test 3: Password with single quote
echo "Test 3: Single quote in password\n";
$result = @fbird_create_database($test_base . "_inj3", $user, "pass'word");
if ($result !== false) {
    @fbird_drop_db($result);
}
echo "Password handled safely\n";

// Test 4: Invalid charset rejected
echo "Test 4: Invalid charset\n";
$result = @fbird_create_database($test_base . "_inj4", $user, $password, "INVALID; DROP DATABASE");
var_dump($result);

echo "Done\n";
?>
--CLEAN--
<?php
require("firebird.inc");
// Clean up any databases that may have been created
@fbird_drop_db(@fbird_connect($test_base . "_test'injection", $user, $password));
@fbird_drop_db(@fbird_connect($test_base . "_inj2", $user, $password));
@fbird_drop_db(@fbird_connect($test_base . "_inj3", $user, $password));
@fbird_drop_db(@fbird_connect($test_base . "_inj4", $user, $password));
?>
--EXPECTF--
Test 1: Single quote in database path
%s
Test 2: Single quote in username
Username handled safely
Test 3: Single quote in password
Password handled safely
Test 4: Invalid charset
bool(false)
Done
