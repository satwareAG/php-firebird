--TEST--
Firebird\TransactionManager userland wrapper: begin, commit, rollback, savepoints, query
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';
require_once __DIR__ . '/../vendor/autoload.php';

use Firebird\Database;
use Firebird\TransactionManager;

$conn = fbird_connect($test_base, $user, $password);
$db = Database::fromResource($conn, $test_base);

// begin() static factory
$tr = TransactionManager::begin($db->getResource());
var_dump($tr instanceof TransactionManager);
var_dump($tr->isActive());
var_dump($tr->isCommitted() === false);
var_dump($tr->isRolledBack() === false);

// getResource()
var_dump($tr->getResource() !== null);

// query() within transaction
$result = $tr->query('SELECT 1 FROM RDB$DATABASE');
var_dump($result !== false);

// commit()
var_dump($tr->commit());
var_dump($tr->isActive() === false);
var_dump($tr->isCommitted());

// rollback()
$tr2 = TransactionManager::begin($db->getResource());
var_dump($tr2->isActive());
var_dump($tr2->rollback());
var_dump($tr2->isRolledBack());

// commitRetaining()
$tr3 = TransactionManager::begin($db->getResource());
var_dump($tr3->commitRetaining());
var_dump($tr3->isActive());

// rollbackRetaining()
var_dump($tr3->rollbackRetaining());
var_dump($tr3->isActive());
$tr3->rollback();

// savepoints
$tr4 = TransactionManager::begin($db->getResource());
$tr4->savepoint('sp1');
$sps = $tr4->getSavepoints();
var_dump(in_array('sp1', $sps));
$tr4->rollbackToSavepoint('sp1');
$tr4->releaseSavepoint('sp1');
$tr4->commit();

// Database::beginTransaction() integration
$tr6 = $db->beginTransaction();
var_dump($tr6 instanceof TransactionManager);
var_dump($tr6->isActive());
$tr6->rollback();

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
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
done
