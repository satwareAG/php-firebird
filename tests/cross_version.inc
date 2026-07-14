<?php
/**
 * Cross-version Firebird connection helper.
 *
 * Auto-detects available Firebird server versions from docker-compose services
 * and provides connection helpers for cross-version compatibility tests.
 *
 * Usage in tests:
 *   require_once __DIR__ . '/../cross_version.inc';
 *   $conn = cross_version_connect('3.0');   // connect to FB 3.0 server
 *   skip_if_fb_server_not_available('3.0');  // skip if FB 3.0 not running
 *
 * @package php-firebird
 * @since   12.1.0
 */

declare(strict_types=1);

/**
 * Mapping of Firebird version strings to docker-compose service names.
 *
 * Service names match docker/docker-compose.yml. Tests connect via the docker
 * network service name (not localhost:port) to avoid ambiguity when multiple
 * FB versions are running or when a native FB server occupies port 3050.
 *
 * After connecting, the actual server version is verified to ensure we're
 * talking to the right FB version, not just any server that responds.
 */
function cross_version_get_service_map(): array
{
    return [
        '2.5' => ['service' => 'firebird25', 'port' => 3051, 'db_path' => '/firebird/data/test.fdb', 'image' => 'jacobalberty/firebird:v2.5.9-ss-jessie'],
        '3.0' => ['service' => 'firebird30', 'port' => 3050, 'db_path' => '/var/lib/firebird/data/test.fdb', 'image' => 'firebirdsql/firebird:3'],
        '4.0' => ['service' => 'firebird40', 'port' => 3050, 'db_path' => '/var/lib/firebird/data/test.fdb', 'image' => 'firebirdsql/firebird:4'],
        '5.0' => ['service' => 'firebird50', 'port' => 3050, 'db_path' => '/var/lib/firebird/data/test.fdb', 'image' => 'firebirdsql/firebird:5'],
    ];
}

/**
 * Extract the major.minor version string from a Firebird server version string.
 * Handles formats like "LI-V3.0.13.33818 Firebird 3.0" and "5.0".
 */
function cross_version_extract_version(string $versionString): ?string
{
    // Match patterns like "3.0", "4.0.2", "5.0.1" anywhere in the string
    if (preg_match('/\b(\d+\.\d+)\./', $versionString, $m)) {
        return $m[1];
    }
    if (preg_match('/\b(\d+\.\d+)\b/', $versionString, $m)) {
        return $m[1];
    }
    return null;
}

/**
 * Check if a specific Firebird server version is reachable via docker-compose.
 *
 * Connects via the docker-compose service name and verifies the actual
 * server version matches the requested version.
 *
 * @param string $version Firebird version string ('3.0', '4.0', '5.0')
 * @return bool True if the server is reachable and version matches
 */
function is_fb_server_available(string $version): bool
{
    $services = cross_version_get_service_map();
    if (!isset($services[$version])) {
        return false;
    }

    $service = $services[$version]['service'];
    $port = $services[$version]['port'] ?? 3050;
    $dbPath = $services[$version]['db_path'] ?? '/firebird/data/test.fdb';

    // Try docker-compose service name first (when running inside the docker network,
    // all FB containers use port 3050 internally)
    $conn = @fbird_connect("{$service}:{$dbPath}", 'SYSDBA', 'masterkey');
    if (!$conn) {
        // Fallback to localhost with version-specific port (for host-side testing
        // with standalone containers that expose different ports)
        $conn = @fbird_connect("localhost/{$port}:{$dbPath}", 'SYSDBA', 'masterkey');
    }

    if (!$conn) {
        return false;
    }

    // Verify the actual server version matches
    $rs = @fbird_query($conn, "SELECT rdb\$get_context('SYSTEM', 'ENGINE_VERSION') FROM rdb\$database");
    if ($rs) {
        $row = fbird_fetch_row($rs);
        fbird_free_result($rs);
        $serverVersion = cross_version_extract_version($row[0] ?? '');
        fbird_close($conn);
        return $serverVersion === $version;
    }

    // If ENGINE_VERSION not available (FB < 3.0), fall back to service name match
    fbird_close($conn);
    return true;
}


/**
 * Connect to a specific Firebird server version.
 *
 * @param string $version Firebird version ('3.0', '4.0', '5.0')
 * @param string $user Database username (default SYSDBA)
 * @param string $pass Database password (default masterkey)
 * @param string $charset Character set (default UTF8)
 * @return Firebird\Connection
 * @throws Exception If server is not available or connection fails
 */
function cross_version_connect(
    string $version,
    string $user = 'SYSDBA',
    string $pass = 'masterkey',
    string $charset = 'UTF8'
): \Firebird\Connection {
    $services = cross_version_get_service_map();
    if (!isset($services[$version])) {
        throw new Exception("Unknown Firebird version: {$version}");
    }

    $service = $services[$version]['service'];
    $port = $services[$version]['port'] ?? 3050;
    $dbPath = $services[$version]['db_path'] ?? '/firebird/data/test.fdb';

    // Try docker-compose service name first
    $dsn = "{$service}:{$dbPath}";
    $conn = @fbird_connect($dsn, $user, $pass, $charset);

    if (!$conn) {
        // Fallback to localhost with version-specific port
        $dsn = "localhost/{$port}:{$dbPath}";
        $conn = @fbird_connect($dsn, $user, $pass, $charset);
    }

    if (!$conn) {
        throw new Exception(
            "Cannot connect to Firebird {$version} server: " . fbird_errmsg()
        );
    }

    return $conn;
}
