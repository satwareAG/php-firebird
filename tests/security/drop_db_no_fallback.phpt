--TEST--
SEC FR-007 (#731): fbird_drop_db() string overload must not fall back to hardcoded SYSDBA/masterkey - omitted credentials fail loudly; explicit credentials unchanged
--CREDITS--
v13.4.0 security hardening train (#728) - FR-007 wire observer
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
$host = getenv('FIREBIRD_HOST') ?: 'localhost';

/* 1. string-mode WITHOUT credentials: must fail with a module validation
 *    error. RED anchor: on unpatched code the attach is silently ATTEMPTED
 *    with hardcoded SYSDBA/masterkey - against a default-credential server
 *    the login SUCCEEDS and the error becomes the engine's I/O error. */
$r = @fbird_drop_db($host . ':/tmp/nonexistent_731.fdb');
echo 'no-creds rejected: ' . ($r === false ? 'yes' : 'NO') . "\n";
echo 'no-creds error: ' . (strpos(fbird_errmsg(), 'credentials are required') !== false ? 'ok' : 'FAIL: ' . fbird_errmsg()) . "\n";

/* 2. string-mode WITH explicit (bad) credentials: still an engine-level auth
 *    error - the explicit-credentials contract is unchanged. NOTE: drop_db's
 *    handwritten arginfo declares args 2/3 by-reference (legacy heritage), so
 *    fixtures must pass variables, not literals. */
$dsn = $host . ':/tmp/nonexistent_731.fdb';
$u = 'SYSDBA';
$p = 'wrong-password-731';
$r2 = @fbird_drop_db($dsn, $u, $p);
echo 'explicit bad creds: engine error: ' . ($r2 === false && strpos(fbird_errmsg(), 'credentials are required') === false ? 'ok' : 'FAIL: ' . fbird_errmsg()) . "\n";

/* 3. password omitted (partial omission) - also rejected */
$r3 = @fbird_drop_db($dsn, $u);
echo 'password-omitted rejected: ' . ($r3 === false && strpos(fbird_errmsg(), 'credentials are required') !== false ? 'ok' : 'FAIL: ' . fbird_errmsg()) . "\n";

echo "=== DONE ===\n";
?>
--EXPECT--
no-creds rejected: yes
no-creds error: ok
explicit bad creds: engine error: ok
password-omitted rejected: ok
=== DONE ===
