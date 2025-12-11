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
		die('skip: cannot create temporary database file in FIREBIRD_DB_DIR');
	}
	@unlink($tmp);
	$test_base = !empty($host) ? $host . ':' . $tmp : $tmp;
} elseif ($fbDbPathEnv !== false && $fbDbPathEnv !== '') {
	$test_base = !empty($host) ? $host . ':' . $fbDbPathEnv : $fbDbPathEnv;
} else {
	unlink($test_base = tempnam(sys_get_temp_dir(), "php_fbird_test"));
	if (!empty($host)) {
		$test_base = $host . ':' . $test_base;
	}
}

function init_db()
{
	global $test_base, $user, $password;

	$sql = sprintf("CREATE SCHEMA '%s' USER '%s' PASSWORD '%s' DEFAULT CHARACTER SET %s",
		$test_base, $user, $password,
		($charset = ini_get('fbird.default_charset')) ? $charset : 'NONE'
	);

	$test_db = fbird_query(FBIRD_CREATE, $sql);

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
