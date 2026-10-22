<?php
// tests/windows/shutdown_smoke.php - #585 regression probe
//
// Crash class: 0xC0000005 (STATUS_ACCESS_VIOLATION, exit code -1073741819)
// during PHP shutdown AFTER Firebird connections were opened and closed.
// The process output completes normally; the crash hits in request/module
// shutdown. Therefore THIS SCRIPT'S EXIT CODE IS THE TEST:
//   0                = clean shutdown (pass)
//   2                = assertion failure inside the script (harness bug)
//   -1073741819      = the #585 crash class (fail; CI step dumps the
//                      Windows Application Error event for the faulting
//                      module + offset)
//
// Exercises all three API layers: fbird_* procedural, and pdo_fbird.
// Multiple connections (including a close-while-open pattern) mirror the
// consumer suite that first exposed the crash (#585 / driver#167).

error_reporting(E_ALL);

function smoke_fail(string $msg): never
{
    fwrite(STDERR, "SMOKE-FAIL: $msg\n");
    exit(2);
}

$db = getenv('SMOKE_DB') ?: 'localhost:C:\\firebird_tests\\smoke.fdb';
$user = getenv('ISC_USER') ?: 'SYSDBA';
$pass = getenv('ISC_PASSWORD') ?: 'masterkey';

// --- Layer 1: procedural API, connection + DML + fetch + close -------------
$conn = @fbird_connect($db, $user, $pass, 'UTF8');
if ($conn === false) {
    smoke_fail('fbird_connect: ' . fbird_errmsg());
}
$res = fbird_query($conn, 'SELECT 1 FROM RDB$DATABASE');
if ($res === false) {
    smoke_fail('fbird_query: ' . fbird_errmsg());
}
$row = fbird_fetch_row($res);
if ($row === false || (int) $row[0] !== 1) {
    smoke_fail('fbird_fetch_row: ' . var_export($row, true));
}
fbird_free_result($res);

// Second connection on the same process, closed explicitly (the consumer
// pattern: several short-lived connections per suite).
$conn2 = fbird_connect($db, $user, $pass, 'UTF8');
if ($conn2 === false) {
    smoke_fail('fbird_connect #2: ' . fbird_errmsg());
}
fbird_close($conn2);
fbird_close($conn);

// --- Layer 3: PDO driver ----------------------------------------------------
$pdo = new PDO('fbird:dbname=' . $db . ';charset=UTF8', $user, $pass);
$ok = $pdo->query('SELECT 1 FROM RDB$DATABASE')->fetchColumn();
if ((int) $ok !== 1) {
    smoke_fail('pdo fetch: ' . var_export($ok, true));
}
$pdo = null;

echo 'SMOKE-OK: connections opened, used, closed on fbird_* and pdo_fbird; shutdown follows' . PHP_EOL;
exit(0);
