<?php

require_once('config.inc');
require_once('functions.inc');

/* we need just the generated name, not the file itself */
/*
 * Docker-aware database location strategy:
 * - If FIREBIRD_DB_DIR is set, create a unique database file name inside that
 *   directory. This is intended for setups where the PHP container and the
 *   Firebird container share a volume (e.g. mounted at /firebird).
 * - Else if FIREBIRD_DB_PATH is set, use it as-is (legacy behaviour).
 * - Else fall back to a temp file in sys_get_temp_dir() as originally.
 */
$fbDbDirEnv  = getenv('FIREBIRD_DB_DIR');
$fbDbPathEnv = getenv('FIREBIRD_DB_PATH');

/*
 * Ensure Firebird client environment variables are set for authentication.
 * This provides a robust fallback if credentials in the connection string
 * (e.g. in CREATE DATABASE) are not parsed correctly by the client library,
 * which has been observed in some environment variations (e.g. PHP 8.5).
 */
if ($user) {
    putenv("ISC_USER=$user");
}
if ($password) {
    putenv("ISC_PASSWORD=$password");
}

if ($fbDbDirEnv !== false && $fbDbDirEnv !== '') {
	$dbDir = rtrim($fbDbDirEnv, '/');
	/*
	 * Generate a unique database file name WITHOUT creating a client-side file.
	 * The previous tempnam() approach created a file on the PHP host's filesystem,
	 * which fails when PHP runs on the host and Firebird runs in a container
	 * (host path != server path). The Firebird server creates the DB file when
	 * CREATE DATABASE executes, so we only need the name string.
	 *
	 * Per-test unique names eliminate test isolation bugs: each test gets its
	 * own database, so leftover state from one test cannot affect another.
	 */
	$tmp = $dbDir . '/php_fbird_test_' . bin2hex(random_bytes(8));
	$test_base = !empty($host) ? $host . ':' . $tmp : $tmp;
} elseif ($fbDbPathEnv !== false && $fbDbPathEnv !== '') {
	$test_base = !empty($host) ? $host . ':' . $fbDbPathEnv : $fbDbPathEnv;
} else {
	/*
	 * Firebird server restricts database creation paths via DatabaseAccess
	 * configuration. Try common Firebird data directories first, then fall
	 * back to system temp dir. CI environments (IBSurgeon installs) typically
	 * allow /opt/firebird/data but may block /tmp.
	 */
	$candidateDirs = [
		'/opt/firebird/data',
		'/var/lib/firebird/data',
		'/var/lib/firebird',
		sys_get_temp_dir()
	];
	$tmp = false;
	foreach ($candidateDirs as $dir) {
		if (is_dir($dir) && is_writable($dir)) {
			$tmp = @tempnam($dir, 'php_fbird_test_');
			if ($tmp !== false) {
				break;
			}
		}
	}
	if ($tmp === false) {
		// Last resort - try system temp anyway
		$tmp = tempnam(sys_get_temp_dir(), 'php_fbird_test_');
	}
	@unlink($tmp);
	$test_base = !empty($host) ? $host . ':' . $tmp : $tmp;
}

function init_db()
{
	global $test_base, $user, $password;

	$sql = sprintf("CREATE SCHEMA '%s' USER '%s' PASSWORD '%s' DEFAULT CHARACTER SET %s",
		$test_base, $user, $password,
		($charset = ini_get('fbird.default_charset')) ? $charset : 'NONE'
	);

	// Debug: report initialization attempt and any errors
	// Note: @ suppresses PHP warnings/notices but NOT exceptions. When fbird.enable_exceptions=1,
	// Firebird errors become Firebird\Exception - we must catch those explicitly.
	try {
		$test_db = @fbird_query(FBIRD_CREATE, $sql);
	} catch (\Throwable $e) {
		$test_db = false;
	}
	if ($test_db === false) {
		// Get error before it's cleared
		$err = fbird_errmsg();
		// Try alternate syntax: CREATE DATABASE instead of CREATE SCHEMA
		$sql2 = sprintf("CREATE DATABASE '%s' USER '%s' PASSWORD '%s' DEFAULT CHARACTER SET %s",
			$test_base, $user, $password,
			($charset = ini_get('fbird.default_charset')) ? $charset : 'NONE'
		);
		try {
			$test_db = @fbird_query(FBIRD_CREATE, $sql2);
		} catch (\Throwable $e) {
			$test_db = false;
		}
		if ($test_db === false) {
			// DB might pre-exist (crashed prior run with same unique name —
			// astronomically unlikely with bin2hex(random_bytes(8)) but defensive).
			// Try connecting to the existing DB and recreate test1.
			$err2 = fbird_errmsg();
			$test_db = @fbird_connect($test_base);
			if ($test_db === false) {
				// Only output debug info when TEST_DEBUG is set (avoids polluting normal test output)
				if (getenv('TEST_DEBUG')) {
					fwrite(STDERR, "DEBUG init_db() failed:\n");
					fwrite(STDERR, "  test_base: $test_base\n");
					fwrite(STDERR, "  user: $user\n");
					fwrite(STDERR, "  CREATE SCHEMA error: $err\n");
					fwrite(STDERR, "  CREATE DATABASE error: $err2\n");
					fwrite(STDERR, "  connect error: " . fbird_errmsg() . "\n");
				}
				return;
			}
		}
	}

	// RECREATE TABLE drops+creates atomically (FB 2.0+). Handles both fresh DB
	// and pre-existing DB from a crashed run without separate DROP TABLE.
	$tr = fbird_trans($test_db);
	fbird_query($tr, "RECREATE TABLE test1 (i integer, c varchar(100))");
	fbird_commit_ret($tr);
	fbird_query($tr, "insert into test1(i, c) values(1, 'test table not created with isql')");
	fbird_commit($tr);
	fbird_close($test_db);
}

function cleanup_db()
{
	global $test_base;

	/*
	 * Per-test unique database names (FIREBIRD_DB_DIR mode) mean leftover
	 * databases do not affect other tests — isolation is guaranteed by name,
	 * not by drop. The drop here is a best-effort cleanup.
	 *
	 * CRITICAL: cleanup_db() runs as a user shutdown function (before MSHUTDOWN
	 * where persistent connections are destroyed). If fbird_drop_db() succeeds
	 * while a persistent connection (fbird_pconnect) is still attached, the
	 * subsequent _php_fbird_close_plink() in MSHUTDOWN will SIGSEGV on FB 3.0
	 * because it calls fbc_disconnect() on a dead attachment (issue #260).
	 *
	 * Mitigation: attempt the drop, but never let it corrupt the shutdown path.
	 * The liveness guard in _php_fbird_close_plink() (Phase 2) is the second
	 * line of defense. Here we just try and ignore failures — the unique name
	 * means no isolation issue if the drop fails.
	 */
	if ($r = @fbird_connect($test_base)) {
		@fbird_drop_db($r);
	}
}

function get_fb_version(): float
{
	global $host, $user, $password, $fb_version, $fb_version_str;

	if(isset($fb_version)) return $fb_version;

	if($se = fbird_service_attach($host ? $host : "localhost", $user, $password)) {
		$fb_version_str = fbird_server_info($se, FBIRD_SVC_SERVER_VERSION);
		if($fb_version_str === false)die("skip cannot fbird_server_info()");
		$server_version  = explode(" ", $fb_version_str);
		$vers = (float)$server_version[count($server_version) - 1];
		return $fb_version = $vers;
	} else {
		die("skip: cannot connect");
	}
}

register_shutdown_function('cleanup_db');
init_db();
