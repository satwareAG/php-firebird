--TEST--
Firebird\Database userland wrapper: connect, query, prepare, execute, getInfo, errors
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';
require_once __DIR__ . '/../vendor/autoload.php';

use Firebird\Database;

// Open a dedicated connection for the OO wrapper (fromResource avoids a second fbc_connect allocation)
$_raw_conn = fbird_connect($test_base, $user, $password);
$db = Database::fromResource($_raw_conn, $test_base);
var_dump($db instanceof Database);
var_dump($db->isConnected());
var_dump($db->getDatabasePath() !== '');
var_dump($db->getUsername() === null); // fromResource has no username
var_dump($db->isPersistent() === false);

// getResource()
var_dump($db->getResource() !== null);

// query() - simple SELECT
$result = $db->query('SELECT 1 FROM RDB$DATABASE');
var_dump($result !== false);

// prepare() + execute()
$stmt = $db->prepare('SELECT 1 FROM RDB$DATABASE');
var_dump($stmt !== false);
$res2 = $db->execute($stmt);
var_dump($res2 !== false);

// affectedRows() - returns int
var_dump(is_int($db->affectedRows()));

// getInfo()
$info = $db->getInfo();
var_dump($info instanceof \Firebird\DbInfo);

// getLastError() / getLastErrorCode() - static
$err = Database::getLastError();
var_dump($err === null || is_string($err));
$code = Database::getLastErrorCode();
var_dump($code === null || is_int($code));

echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
done
