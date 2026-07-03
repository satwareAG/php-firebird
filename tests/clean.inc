<?php
/**
 * Garbage-collector for test databases.
 *
 * Used by --CLEAN-- sections in PHPT tests. Does NOT include firebird.inc
 * (which would create a new database via init_db()). Instead, reads marker
 * files written by firebird.inc and drops the corresponding databases.
 *
 * In parallel test execution, each --CLEAN-- invocation cleans up ALL
 * leftover test databases. Dropping an in-use database fails silently
 * (Firebird refuses to drop with active attachments), so there is no
 * race condition.
 */
require_once __DIR__ . '/config.inc';

foreach (glob(sys_get_temp_dir() . '/php_fbird_test_db_*') as $marker) {
	$dbPath = trim((string) @file_get_contents($marker));
	if ($dbPath && ($db = @fbird_connect($dbPath, $user, $password))) {
		if (@fbird_drop_db($db)) {
			@unlink($marker);
		}
		// If drop failed (DB in use by parallel test), leave marker for next run
	} else {
		// DB doesn't exist or can't connect — safe to remove marker
		@unlink($marker);
	}
}

// Also clean up leftover service test backup/restore files
foreach (glob('/tmp/*_bkp_*.fbk') as $f) {
	@unlink($f);
}
foreach (glob('/tmp/*_rst_*.fdb') as $f) {
	@unlink($f);
}
