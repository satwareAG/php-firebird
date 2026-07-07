--TEST--
amicron-platform: Doctrine DBAL SchemaManager listTables / listTableColumns
--CREDITS--
v12.1.0 M3-7 (#358) - amicron-platform customer integration test (RED)
--SKIPIF--
<?php
require_once __DIR__ . '/skipif.inc';
// This test documents the gap: fbird_meta_data() / fbird_list_tables() don't exist yet
// (issue #373, M2 milestone). When they're implemented, this test should pass green.
// For now it verifies the RDB$* system table queries that Doctrine uses internally.
?>
--FILE--
<?php
/**
 * Test that the RDB$* system table queries used by Doctrine's
 * FirebirdSchemaManager return correct results through the driver.
 *
 * Doctrine's SchemaManager queries RDB$RELATIONS, RDB$RELATION_FIELDS,
 * and RDB$FIELDS directly (because the driver doesn't expose
 * fbird_meta_data() / fbird_list_tables() yet — see issue #373).
 *
 * This test verifies those queries work and return expected data shapes.
 */
$DB = 'localhost/3050:/var/lib/firebird/data/amicron-demo.fdb';
$USER = 'AMICRON03';
$PASS = 'klopf';
$CHARSET = 'ISO8859_1';

echo "=== Doctrine SchemaManager queries ===\n";

$pdo = new PDO(
    "fbird:host=localhost;dbname=/var/lib/firebird/data/amicron-demo.fdb;charset=ISO8859_1",
    $USER, $PASS,
    [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]
);

// --- 1. listTables: query RDB$RELATIONS for user tables ---
$stmt = $pdo->query("
    SELECT TRIM(rdb\$relation_name) AS table_name
    FROM rdb\$relations
    WHERE rdb\$view_blr IS NULL
      AND (rdb\$system_flag = 0 OR rdb\$system_flag IS NULL)
    ORDER BY rdb\$relation_name
");
$tables = $stmt->fetchAll(PDO::FETCH_COLUMN);
$table_count = count($tables);
echo "OK listTables: $table_count user tables\n";

// Verify key tables are present
$expected_tables = ['ADRESSEN', 'AUFTRAG', 'ARTIKEL', 'ATRPOS', 'KONTAKTE'];
foreach ($expected_tables as $t) {
    if (in_array($t, $tables)) {
        echo "OK table present: $t\n";
    } else {
        echo "FAIL table missing: $t\n";
    }
}

// --- 2. listTableColumns for ADRESSEN ---
$stmt = $pdo->query("
    SELECT
        TRIM(rf.rdb\$field_name) AS column_name,
        f.rdb\$field_type AS field_type,
        f.rdb\$field_length AS length,
        rf.rdb\$null_flag AS not_null,
        rf.rdb\$default_source AS default_value
    FROM rdb\$relation_fields rf
    JOIN rdb\$fields f ON rf.rdb\$field_source = f.rdb\$field_name
    WHERE UPPER(TRIM(rf.rdb\$relation_name)) = 'ADRESSEN'
    ORDER BY rf.rdb\$field_position
");
$columns = $stmt->fetchAll(PDO::FETCH_ASSOC);
$col_count = count($columns);
echo "OK listTableColumns(ADRESSEN): $col_count columns\n";

// Verify key columns exist
$col_names = array_column($columns, 'COLUMN_NAME');
$expected_cols = ['LFDNR', 'NR', 'NAME', 'ORT', 'STRASSE'];
foreach ($expected_cols as $c) {
    if (in_array($c, $col_names)) {
        echo "OK column present: $c\n";
    } else {
        echo "FAIL column missing: $c\n";
    }
}

// --- 3. Verify column type mapping ---
$lfdnr = $columns[0]; // LFDNR should be first
echo "OK LFDNR type: " . $lfdnr['FIELD_TYPE'] . " (37=VARCHAR, 8=INTEGER, etc.)\n";

$pdo = null;
echo "OK disconnect\n";

echo "=== DONE ===\n";
?>
--CLEAN--
<?php // No DDL changes - read-only queries only ?>
--EXPECTF--
=== Doctrine SchemaManager queries ===
OK listTables: %d user tables
OK table present: ADRESSEN
OK table present: AUFTRAG
OK table present: ARTIKEL
OK table present: ATRPOS
OK table present: KONTAKTE
OK listTableColumns(ADRESSEN): %d columns
OK column present: LFDNR
OK column present: NR
OK column present: NAME
OK column present: ORT
OK column present: STRASSE
OK LFDNR type: %s (37=VARCHAR, 8=INTEGER, etc.)
OK disconnect
=== DONE ===
