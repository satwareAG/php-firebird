<?php

// ============================================================================
// Service Manager Teardown Pattern (OC-10)
// ============================================================================
// When a test calls FBIRD_RPR_VALIDATE_DB and the database is marked as
// "damaged", subsequent tests inherit that damaged state. The correct
// teardown sequence is:
//
//   1. FBIRD_RPR_VALIDATE_DB + FBIRD_RPR_FULL  — check for errors
//   2. FBIRD_RPR_MEND_DB    + FBIRD_RPR_FULL  — repair if needed
//   3. FBIRD_RPR_VALIDATE_DB + FBIRD_RPR_FULL  — verify no remaining errors
//
// See fbird_service_validate_mend_cycle.phpt for a working example.
// ============================================================================

function test_use_after_fbird_free_query() {
	$batch_count = 5;

	fbird_query("DELETE FROM TEST1");
	$query = fbird_prepare("INSERT INTO TEST1 (I, C) VALUES (?, ?)");
	for($i = 1; $i <= $batch_count; $i++) {
		fbird_execute($query, $i, "ROW 1 (batch $i)");
		fbird_execute($query, $i, "ROW 2 (batch $i)");
	}
	fbird_free_query($query);

	$query = fbird_prepare("SELECT * FROM TEST1 WHERE I = ?");
	$res = array();
	for($i = 1; $i <= $batch_count; $i++) {
		$res[$i] = fbird_execute($query, $i);
	}
	fbird_free_query($query);

	foreach ($res as $batch=>$r) {
		print "---- Batch $batch ----\n";
		dump_rows($r);
		fbird_free_result($r);
	}
}

function test_fbird_trans_014_015() {
	global $test_base;

	$x = fbird_connect($test_base);
	var_dump(fbird_trans($x));
	var_dump(fbird_trans(1));
	var_dump(fbird_close());
	var_dump(fbird_close($x));
}

function test_time_unixtime(): void {
	fbird_query("CREATE TABLE TTEST (
		ID INTEGER,
		T1 TIME DEFAULT '15:45:59',
		T2 TIMESTAMP DEFAULT '2025-11-06 15:45:59'
	)");
	fbird_commit();
	fbird_query("INSERT INTO TTEST (ID) VALUES (1)");
	dump_table_rows("TTEST");
	dump_table_rows("TTEST", null, FBIRD_UNIXTIME);
}

function test_time_tz_unixtime(): void {
	fbird_query("CREATE TABLE TTEST (
		ID INTEGER,
		T1 TIME WITH TIME ZONE DEFAULT '15:45:59 UTC',
		T2 TIMESTAMP WITH TIME ZONE DEFAULT '2025-11-06 15:45:59 UTC'
	)");
	fbird_commit();
	fbird_query("INSERT INTO TTEST (ID) VALUES (1)");
	dump_table_rows("TTEST");
	dump_table_rows("TTEST", null, FBIRD_UNIXTIME);
}

function test_fields25(): void {
	fbird_query(file_get_contents(__DIR__."/001-FIELDS25.sql"));
	fbird_commit();

	fbird_query("INSERT INTO FIELDS25 (ID) VALUES (1)");
	$q = fbird_query("SELECT * FROM FIELDS25");
	$num_fields = fbird_num_fields($q);
	for($i = 0; $i < $num_fields; $i++){
		$info = fbird_field_info($q, $i);
		printf("%s/%s/%d\n", $info["name"], $info["type"], $info["length"]);
	}
}

function test_fields40(): void {
	fbird_query(file_get_contents(__DIR__."/001-FIELDS40.sql"));
	fbird_commit();

	fbird_query("INSERT INTO FIELDS40 (ID) VALUES (DEFAULT)");
	$q = fbird_query("SELECT * FROM FIELDS40");
	$num_fields = fbird_num_fields($q);
	for($i = 0; $i < $num_fields; $i++){
		$info = fbird_field_info($q, $i);
		printf("%s/%s/%d\n", $info["name"], $info["type"], $info["length"]);
	}
}

function test_field_data40(): void {
	fbird_query(file_get_contents(__DIR__."/001-FIELDS40.sql"));
	fbird_commit();
	fbird_query("INSERT INTO FIELDS40 (ID) VALUES (1)");
	dump_table_rows("FIELDS40");
}

// ============================================================================
// OOP helper functions (OC-6)
// These wrap the Firebird\* OOP API for use in test files.
// Usage: require_once 'common.inc'; $conn = oop_connect();
// ============================================================================

/**
 * Connect to the test database using the OOP API.
 * Auto-includes firebird.inc if $test_base is not yet set.
 *
 * @param string|null $database Override database path (defaults to $test_base)
 * @return \Firebird\Connection
 * @throws \Firebird\DatabaseException On connection failure
 */
function oop_connect(?string $database = null): \Firebird\Connection
{
	global $test_base, $user, $password;
	if (!isset($test_base)) {
		require_once __DIR__ . '/firebird.inc';
	}
	return new \Firebird\Connection(
		$database ?? $test_base,
		$user,
		$password
	);
}

/**
 * Begin a new transaction on the given connection.
 *
 * @param \Firebird\Connection $conn Active connection
 * @return \Firebird\Transaction
 * @throws \Firebird\TransactionException On failure
 */
function oop_begin_transaction(\Firebird\Connection $conn): \Firebird\Transaction
{
	return $conn->beginTransaction();
}

/**
 * Prepare and execute a SQL statement, returning a ResultSet.
 * Wraps Connection::prepare() + Statement::execute().
 *
 * Note: bind parameters are not supported via this helper because the
 * OOP Statement::execute() only accepts a Transaction (no bind args).
 * For parameterized queries, use $conn->prepare() + fbird_execute().
 *
 * @param \Firebird\Connection $conn Active connection
 * @param \Firebird\Transaction $tx Active transaction
 * @param string $sql SQL text (without placeholders)
 * @return \Firebird\ResultSet
 * @throws \Firebird\QueryException On prepare or execute failure
 */
function oop_query(
	\Firebird\Connection $conn,
	\Firebird\Transaction $tx,
	string $sql
): \Firebird\ResultSet {
	$stmt = $conn->prepare($sql, $tx);
	return $stmt->execute($tx);
}

/**
 * Close a database connection.
 *
 * @param \Firebird\Connection $conn Connection to close
 */
function oop_close(\Firebird\Connection $conn): void
{
	$conn->close();
}
