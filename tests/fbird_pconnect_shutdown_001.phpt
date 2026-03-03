--TEST--
fbird_pconnect: clean shutdown (exit code 0, no SIGSEGV) — regression test for issue #82
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

// Open a persistent connection — this is the specific path that triggered SIGSEGV
// during PHP MSHUTDOWN in rc.51 (issue #82).
$conn = fbird_pconnect($test_base, $user, $password);
if ($conn === false) {
    die("FAIL: fbird_pconnect returned false: " . fbird_errmsg() . "\n");
}

// Verify the connection works
$result = fbird_query($conn, 'SELECT 1 FROM RDB$DATABASE');
if ($result === false) {
    die("FAIL: fbird_query returned false: " . fbird_errmsg() . "\n");
}

$row = fbird_fetch_row($result);
if ($row === false || $row[0] != 1) {
    die("FAIL: unexpected query result\n");
}

fbird_free_result($result);

// Do NOT call fbird_close() — let PHP shutdown handle the persistent connection.
// The SIGSEGV in #82 occurred specifically during MSHUTDOWN when resource
// destructors (_php_fbird_free_service, _php_fbird_free_blob) called
// _php_fbird_error()/_php_fbird_module_error() without IBG(in_mshutdown) guard,
// causing php_error_docref()/zend_throw_exception() to access already-freed EG().

echo "ok\n";
// Script ends here — PHP shutdown will run resource destructors.
// A clean exit (code 0) proves the SIGSEGV is fixed.
?>
--EXPECT--
ok
