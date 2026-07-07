--TEST--
Issue #311: SIGSEGV (exit 139) during module shutdown with persistent connections
--SKIPIF--
<?php require_once('skipif.inc'); ?>
--FILE--
<?php
require_once('firebird.inc');

// Create a persistent connection — this is the crash trigger.
// The persistent connection registers a le_plink resource in EG(persistent_list).
// During module shutdown, zend_destroy_rsrc_list(&EG(persistent_list)) calls
// _php_fbird_close_plink via plist_entry_destructor. The !FBG(in_mshutdown)
// guard is 0 at this point (MSHUTDOWN hasn't run yet), so it allows
// zend_hash_str_del(&EG(regular_list), ...) on freed memory → SIGSEGV.
$conn = fbird_pconnect($test_base, $user, $password);
if (!$conn) {
    echo "FAILED to connect\n";
    exit(1);
}
echo "Connected (persistent)\n";

// Verify the connection works
$res = fbird_query($conn, "SELECT 1 FROM RDB\$DATABASE");
if (!$res) {
    echo "FAILED to query\n";
    exit(1);
}
$row = fbird_fetch_row($res);
echo "Query: $row[0]\n";
fbird_free_result($res);

// Do NOT close the connection — let shutdown handle it.
// This is the exact crash path: persistent link destructor runs during
// zend_destroy_rsrc_list(&EG(persistent_list)), before MSHUTDOWN.
// If the bug is present, PHP exits with code 139 (SIGSEGV) here.
echo "DONE\n";
?>
--EXPECT--
Connected (persistent)
Query: 1
DONE
