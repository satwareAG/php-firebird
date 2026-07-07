<?php
/**
 * verify-firebird-connection.php
 *
 * Verifies that the firebird PHP extension can connect to a Firebird
 * database. Used by CI/CD composite action verify-extension.
 *
 * Environment variables:
 *   FIREBIRD_HOST  - Firebird server hostname
 *   FIREBIRD_DB_PATH - Path to the database file
 *   ISC_USER       - Firebird username (default: SYSDBA)
 *   ISC_PASSWORD   - Firebird password (default: masterkey)
 *
 * Exit codes:
 *   0 - Connection successful
 *   1 - All connection attempts failed
 */

$host = getenv('FIREBIRD_HOST') ?: 'firebird';
$dbPath = getenv('FIREBIRD_DB_PATH') ?: '/var/lib/firebird/data/test.fdb';
$user = getenv('ISC_USER') ?: 'SYSDBA';
$pass = getenv('ISC_PASSWORD') ?: 'masterkey';

$connStr = $host . ':' . $dbPath;

echo "Connection string: $connStr\n";
echo "User: $user\n";

// Try connection with explicit credentials
$db = @fbird_connect($connStr, $user, $pass);
if ($db) {
    echo "SUCCESS: Connected to database!\n";
    $res = fbird_query($db, "SELECT 1 FROM RDB\$DATABASE");
    if ($res) {
        $row = fbird_fetch_row($res);
        echo "Query test: SELECT 1 returned " . $row[0] . "\n";
        fbird_free_result($res);
    }
    fbird_close($db);
    exit(0);
}

$err = fbird_errmsg();
echo "Primary connection failed: $err\n";

// Try alternate database paths (different Docker images use different layouts)
$altPaths = [
    '/var/lib/firebird/data/test.fdb',
    '/firebird/data/test.fdb',
    '/tmp/test.fdb',
];

foreach ($altPaths as $alt) {
    if ($alt === $dbPath) {
        continue;
    }
    $altConn = $host . ':' . $alt;
    echo "Trying alternate: $altConn ... ";
    $db = @fbird_connect($altConn, $user, $pass);
    if ($db) {
        echo "SUCCESS!\n";
        fbird_close($db);
        exit(0);
    }
    echo "failed\n";
}

echo "ERROR: All connection attempts failed\n";
echo "Last error: " . fbird_errmsg() . "\n";
exit(1);
