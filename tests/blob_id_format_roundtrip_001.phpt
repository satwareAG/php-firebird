--TEST--
BLOB ID format: 17-char standard, round-trip, legacy compat (#516)
--CREDITS--
v13.0.1 bugfix audit - #516 blob ID format inconsistency
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
    require("firebird.inc");
    $link = fbird_connect($test_base);

    fbird_query(
        "CREATE TABLE test_blob_id_516 (
            v_blob   BLOB
        )"
    );

    $trx = fbird_trans($link);

    // Create a blob via procedural API
    $blob = fbird_blob_create($trx);
    fbird_blob_add($blob, "test blob data for ID format round-trip");
    $blob_id = fbird_blob_close($blob);

    // The blob ID is returned as a string by fbird_blob_close
    echo "Blob ID type: " . gettype($blob_id) . "\n";
    echo "Blob ID string: " . $blob_id . "\n";

    // Verify format: should be 17 chars (8 hex + colon + 8 hex)
    $len = strlen($blob_id);
    echo "String length: $len\n";

    // Verify format with regex (17-char: 8 hex : 8 hex)
    if (preg_match('/^[0-9a-f]{8}:[0-9a-f]{8}$/', $blob_id)) {
        echo "Format check: PASS (17-char XXXXXXXX:XXXXXXXX)\n";
    } else {
        echo "Format check: FAIL\n";
    }

    // Round-trip: open the blob using the string ID
    $blob2 = fbird_blob_open($trx, $blob_id);
    if ($blob2 === false) {
        echo "Round-trip open: FAIL\n";
    } else {
        $data = "";
        while ($chunk = fbird_blob_get($blob2, 1000)) { $data .= $chunk; }
        fbird_blob_close($blob2);
        echo "Round-trip open: PASS\n";
        echo "Round-trip data: $data\n";
    }

    // Test parameter binding with the new 17-char BLOB ID
    $res = fbird_query($trx, "INSERT INTO test_blob_id_516 (v_blob) VALUES (?)", $blob_id);
    if ($res === false) {
        echo "INSERT with blob_id bind: FAIL\n";
    } else {
        echo "INSERT with blob_id bind: PASS\n";
    }
    fbird_commit($trx);

    // Start a NEW transaction for the SELECT (previous trx is closed after commit)
    $trx2 = fbird_trans($link);

    // Read back the blob via SELECT
    $sel = fbird_query($trx2, "SELECT v_blob FROM test_blob_id_516");
    if ($sel === false) {
        echo "SELECT: FAIL\n";
    } else {
        $row = fbird_fetch_object($sel);
        if ($row && $row->V_BLOB) {
            $blob3 = fbird_blob_open($trx2, $row->V_BLOB);
            $data2 = "";
            while ($chunk = fbird_blob_get($blob3, 1000)) { $data2 .= $chunk; }
            fbird_blob_close($blob3);
            echo "SELECT round-trip: PASS\n";
            echo "SELECT data: $data2\n";
            if ($data2 === "test blob data for ID format round-trip") {
                echo "Data integrity: PASS\n";
            } else {
                echo "Data integrity: FAIL\n";
            }
        } else {
            echo "SELECT: FAIL (no row or no blob)\n";
        }
        fbird_free_result($sel);
    }

    fbird_rollback($trx2);
    fbird_query($link, "DROP TABLE test_blob_id_516");
    fbird_close($link);
    echo "done\n";
?>
--EXPECTF--
Blob ID type: string
Blob ID string: %s:%s
String length: 17
Format check: PASS (17-char XXXXXXXX:XXXXXXXX)
Round-trip open: PASS
Round-trip data: test blob data for ID format round-trip
INSERT with blob_id bind: PASS
SELECT round-trip: PASS
SELECT data: test blob data for ID format round-trip
Data integrity: PASS
done

--CLEAN--
<?php include("clean.inc"); ?>
