--TEST--
Firebird\TBuilder userland wrapper: fluent transaction builder, isolation, wait, reservations
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';
require_once __DIR__ . '/../vendor/autoload.php';

use Firebird\Database;
use Firebird\TBuilder;
use Firebird\TransactionManager;

$conn = fbird_connect($test_base, $user, $password);
$db = Database::fromResource($conn, $test_base);

// create() static factory returns TBuilder
$tb = TBuilder::create();
var_dump($tb instanceof TBuilder);

// fluent isolation methods return self
var_dump($tb->readOnly() instanceof TBuilder);
var_dump($tb->readWrite() instanceof TBuilder);
var_dump($tb->isolationSnapshot() instanceof TBuilder);
var_dump($tb->isolationConcurrency() instanceof TBuilder);
var_dump($tb->isolationReadCommitted() instanceof TBuilder);
var_dump($tb->isolationReadCommittedRecordVersion() instanceof TBuilder);
var_dump($tb->isolationReadCommittedNoRecordVersion() instanceof TBuilder);
var_dump($tb->isolationSnapshotTableStability() instanceof TBuilder);

// wait / noWait
var_dump($tb->wait(5) instanceof TBuilder);
var_dump($tb->getLockTimeout() === 5);
var_dump($tb->noWait() instanceof TBuilder);

// ignoreLimbo, autoCommit, noAutoUndo
var_dump($tb->ignoreLimbo() instanceof TBuilder);
var_dump($tb->autoCommit() instanceof TBuilder);
var_dump($tb->noAutoUndo() instanceof TBuilder);

// table reservations
var_dump($tb->reserveSharedRead('RDB$DATABASE') instanceof TBuilder);
var_dump($tb->reserveSharedWrite('RDB$DATABASE') instanceof TBuilder);
var_dump($tb->reserveProtectedRead('RDB$DATABASE') instanceof TBuilder);
var_dump($tb->reserveProtectedWrite('RDB$DATABASE') instanceof TBuilder);
var_dump(is_array($tb->getTableReservations()));

// build() returns array
var_dump(is_array(TBuilder::create()->isolationReadCommitted()->wait()->build()));

// buildFlags() returns int
var_dump(is_int(TBuilder::create()->readOnly()->buildFlags()));

// copy()
$tb2 = TBuilder::create()->isolationSnapshot()->wait(10);
$tb3 = $tb2->copy();
var_dump($tb3 instanceof TBuilder);
var_dump($tb3->getLockTimeout() === 10);

// hasConnection() before/after connection()
$tb4 = TBuilder::create();
var_dump($tb4->hasConnection() === false);
$tb4->connection($db->getResource());
var_dump($tb4->hasConnection());

// start() returns TransactionManager
$tr = TBuilder::create()
    ->isolationReadCommitted()
    ->readWrite()
    ->wait()
    ->connection($db->getResource())
    ->start();
var_dump($tr instanceof TransactionManager);
var_dump($tr->isActive());
$tr->commit();

// Database::transaction() returns TBuilder with connection set
$tb5 = $db->transaction();
var_dump($tb5 instanceof TBuilder);
var_dump($tb5->hasConnection());

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
