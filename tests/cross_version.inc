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
 *   skip_if_fb_server_not_available('2.5');  // skip if FB 2.5 not running
 *
 * @package php-firebird
 * @since   12.1.0
 */

declare(strict_types=1);

/**
 * Mapping of Firebird version strings to docker-compose service names and ports.
 *
 * The service names match docker/docker-compose.yml. Ports are the host-mapped
 * ports (if exposed). If not exposed, tests connect via the docker network.
 */
function cross_version_get_service_map(): array
{
    return [
        // FB 2.5 deferred: no official Docker image exists (issue #313 reopened, M5 stretch)
        '2.5' => ['service' => 'firebird25', 'port' => 3051, 'image' => 'firebirdsql/firebird:2.5-sbr'],
        '3.0' => ['service' => 'firebird30', 'port' => 3050, 'image' => 'firebirdsql/firebird:3.0'],
        '4.0' => ['service' => 'firebird40', 'port' => 3050, 'image' => 'firebirdsql/firebird:4.0'],
        '5.0' => ['service' => 'firebird50', 'port' => 3050, 'image' => 'firebirdsql/firebird:5.0'],
        '6.0' => ['service' => 'firebird60', 'port' => 3050, 'image' => 'firebirdsql/firebird:6.0'],
    ];
}

/**
 * Check if a specific Firebird server version is reachable.
 *
 * Tries to connect to the service on localhost or the docker-compose service name.
 * Falls back gracefully if the service is not running.
 *
 * @param string $version Firebird version string ('2.5', '3.0', '4.0', '5.0', '6.0')
 * @return bool True if the server is reachable
 */
function is_fb_server_available(string $version): bool
{
    $services = cross_version_get_service_map();
    if (!isset($services[$version])) {
        return false;
    }

    $port = $services[$version]['port'];

    // Try localhost first (host-mapped port)
    $host = 'localhost';
    $connected = @fbird_connect("{$host}/{$port}:/firebird/data/test.fdb", 'SYSDBA', 'masterkey');

    if ($connected) {
        fbird_close($connected);
        return true;
    }

    // Try docker-compose service name (when running inside the docker network)
    $service = $services[$version]['service'];
    $connected = @fbird_connect("{$service}:/firebird/data/test.fdb", 'SYSDBA', 'masterkey');

    if ($connected) {
        fbird_close($connected);
        return true;
    }

    return false;
}

/**
 * Get list of available Firebird server versions.
 *
 * @return array Array of version strings ('2.5', '3.0', etc.)
 */
function get_available_fb_versions(): array
{
    $available = [];
    foreach (array_keys(cross_version_get_service_map()) as $version) {
        if (is_fb_server_available($version)) {
            $available[] = $version;
        }
    }
    return $available;
}

/**
 * Connect to a specific Firebird server version.
 *
 * @param string $version Firebird version ('2.5', '3.0', '4.0', '5.0', '6.0')
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

    $port = $services[$version]['port'];

    // Try localhost first (host-mapped port)
    $dsn = "localhost/{$port}:/firebird/data/test.fdb";
    $conn = @fbird_connect($dsn, $user, $pass, $charset);

    if (!$conn) {
        // Try docker-compose service name
        $service = $services[$version]['service'];
        $dsn = "{$service}:/firebird/data/test.fdb";
        $conn = @fbird_connect($dsn, $user, $pass, $charset);
    }

    if (!$conn) {
        throw new Exception(
            "Cannot connect to Firebird {$version} server: " . fbird_errmsg()
        );
    }

    return $conn;
}

/**
 * Skip the current test if a specific Firebird server version is not available.
 *
 * For use in --SKIPIF-- sections of cross-version tests.
 *
 * @param string $version Firebird version ('2.5', '3.0', '4.0', '5.0', '6.0')
 */
function skip_if_fb_server_not_available(string $version): void
{
    if (!is_fb_server_available($version)) {
        die("skip Firebird {$version} server not available");
    }
}
