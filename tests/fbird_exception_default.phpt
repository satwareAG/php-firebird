--TEST--
v9: FBIRD_EXCEPTION_MODE_COMPAT constant and THROW mode works
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
?>
--FILE--
<?php
/* Verify default is SILENT for backward compat */
$mode = fbird_get_exception_mode();
echo "default mode: " . $mode . "\n";
echo "is SILENT: " . ($mode === FBIRD_EXCEPTION_MODE_SILENT ? "yes" : "no") . "\n";

/* Verify COMPAT constant exists and equals SILENT */
echo "COMPAT == SILENT: " . (FBIRD_EXCEPTION_MODE_COMPAT === FBIRD_EXCEPTION_MODE_SILENT ? "yes" : "no") . "\n";

/* Switch to THROW and verify that errors throw */
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_THROW);
try {
    fbird_connect('nonexistent_host_xyz:/no/such/db.fdb', 'SYSDBA', 'masterkey');
    echo "no exception\n";
} catch (\Throwable $e) {
    echo "caught: " . get_class($e) . "\n";
}

/* Switch back to SILENT */
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_SILENT);
echo "mode after set SILENT: " . fbird_get_exception_mode() . "\n";

echo "Done\n";
?>
--EXPECTF--
default mode: 0
is SILENT: yes
COMPAT == SILENT: yes
caught: Firebird\Exception
mode after set SILENT: 0
Done
