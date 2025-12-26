<?php
/**
 * Basic AddressSanitizer Verification Script
 * 
 * This script performs basic Firebird operations to ensure
 * no immediate memory errors occur during connection and simple queries.
 */

require_once __DIR__ . '/../firebird.inc';

echo "ASan Basic Test: Starting...\n";

// 1. Connect
$dbh = fbird_connect($test_base, $user, $password);
if (!$dbh) {
    die("Failed to connect: " . fbird_errmsg());
}
echo "Connected.\n";

// 2. Simple Query
$query = "SELECT 1 FROM RDB\$DATABASE";
$res = fbird_query($dbh, $query);
if (!$res) {
    die("Query failed: " . fbird_errmsg());
}

while ($row = fbird_fetch_object($res)) {
    // Just fetch
}
fbird_free_result($res);
echo "Query executed.\n";

// 3. Close
fbird_close($dbh);
echo "ASan Basic Test: Completed.\n";
