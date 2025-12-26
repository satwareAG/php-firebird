<?php

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
	ini_set("ibase.timeformat", "%H:%M:%S");
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
