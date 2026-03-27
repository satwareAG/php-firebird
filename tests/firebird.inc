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
	$tmp   = @tempnam($dbDir, 'php_fbird_test_');
	if ($tmp === false) {
		// FIREBIRD_DB_DIR may be on remote server, fall back to local /tmp
		$tmp = tempnam(sys_get_temp_dir(), 'php_fbird_test_');
	}
	@unlink($tmp);
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
			// Still failed - write debug info to stderr
			$err2 = fbird_errmsg();
			// Only output debug info when TEST_DEBUG is set (avoids polluting normal test output)
			if (getenv('TEST_DEBUG')) {
				fwrite(STDERR, "DEBUG init_db() failed:\n");
				fwrite(STDERR, "  test_base: $test_base\n");
				fwrite(STDERR, "  user: $user\n");
				fwrite(STDERR, "  CREATE SCHEMA error: $err\n");
				fwrite(STDERR, "  CREATE DATABASE error: $err2\n");
			}
			// Return without creating tables - database doesn't exist
			return;
		}
	}

	$tr = fbird_trans($test_db);
	fbird_query($tr,"create table test1 (i integer, c varchar(100))");
	fbird_commit_ret($tr);
	fbird_query($tr,"insert into test1(i, c) values(1, 'test table not created with isql')");
	fbird_commit($tr);
	fbird_close($test_db);
}

function cleanup_db()
{
	global $test_base;

	/*
	 * During SKIPIF and normal shutdown, the test harness may attempt to drop
	 * the test database while other transactions are still active. That can
	 * lead to lock time-out warnings, which in SKIPIF context cause tests to
	 * be BORKed ("invalid output from SKIPIF"). We ignore failures here to
	 * keep the test output clean; any leftover database will be cleaned up on
	 * subsequent runs once no transactions are holding locks.
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
