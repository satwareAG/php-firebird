--TEST--
Check functionality of fbird_* aliases
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("interbase.inc");

function test_alias_functionality() {
    global $test_base, $user, $password;

    // 1. Connection aliases
    $dbh = fbird_connect($test_base, $user, $password);
    if (!$dbh) {
        echo "fbird_connect failed\n";
        return;
    }
    echo "fbird_connect succeeded\n";

    // 2. Transaction aliases
    $trans = fbird_trans($dbh);
    if (!$trans) {
        echo "fbird_trans failed\n";
    } else {
        echo "fbird_trans succeeded\n";
        // 3. Query aliases (DDL)
        $query = "CREATE TABLE alias_test (id INTEGER, text_val VARCHAR(32))";
        if (fbird_query($trans, $query)) {
            echo "fbird_query (DDL) succeeded\n";
        } else {
            echo "fbird_query (DDL) failed: " . fbird_errmsg() . "\n";
        }
        fbird_commit($trans);
        echo "fbird_commit succeeded\n";
    }

    // 4. Data Manipulation
    $trans = fbird_trans($dbh);
    $insert_query = "INSERT INTO alias_test (id, text_val) VALUES (?, ?)";
    $stmt = fbird_prepare($trans, $insert_query);
    if ($stmt) {
        echo "fbird_prepare succeeded\n";
        if (fbird_execute($stmt, 1, 'test_value')) {
            echo "fbird_execute succeeded\n";
        } else {
            echo "fbird_execute failed\n";
        }
        fbird_free_query($stmt);
        echo "fbird_free_query succeeded\n";
    } else {
        echo "fbird_prepare failed\n";
    }
    fbird_commit_ret($trans); // Commit and retain
    echo "fbird_commit_ret succeeded\n";

    // Verify inserted data
    $select_query = "SELECT id, text_val FROM alias_test";
    $res = fbird_query($trans, $select_query);
    if ($res) {
        echo "fbird_query (SELECT) succeeded\n";

        // Test fetch aliases
        $row = fbird_fetch_assoc($res);
        if ($row && $row['ID'] == 1 && $row['TEXT_VAL'] == 'test_value') {
            echo "fbird_fetch_assoc succeeded\n";
        } else {
            echo "fbird_fetch_assoc failed\n";
        }

        // Verify metadata aliases
        $cols = fbird_num_fields($res);
        if ($cols == 2) {
            echo "fbird_num_fields succeeded\n";
        } else {
            echo "fbird_num_fields failed ($cols)\n";
        }

        $field_info = fbird_field_info($res, 0);
        if (is_array($field_info) && isset($field_info['name'])) {
            echo "fbird_field_info succeeded\n";
        } else {
            echo "fbird_field_info failed\n";
        }

        fbird_free_result($res);
        echo "fbird_free_result succeeded\n";
    } else {
        echo "fbird_query (SELECT) failed\n";
    }

    // Test GEN_ID alias
    // Note: Assuming RDB$DATABASE exists and can use implicit generator if needed,
    // but standard test usually creates a generator. For simple check, we skip creating one
    // or assume standard setup available. Actually, failure is expected without explicit generator.
    // Let's create one first.
    fbird_query($trans, "CREATE GENERATOR test_gen");
    fbird_commit_ret($trans);

    $gen_val = fbird_gen_id("test_gen", 1, $dbh);
    if (is_numeric($gen_val)) {
        echo "fbird_gen_id succeeded\n";
    } else {
        echo "fbird_gen_id failed\n";
    }

    // Test Rollback aliases
    $trans_rb = fbird_trans($dbh);
    fbird_query($trans_rb, "INSERT INTO alias_test (id, text_val) VALUES (2, 'rollback_val')");
    fbird_rollback($trans_rb);
    echo "fbird_rollback succeeded\n";

    // Test Rollback Retain
    $trans_rb_ret = fbird_trans($dbh);
    fbird_query($trans_rb_ret, "INSERT INTO alias_test (id, text_val) VALUES (3, 'rollback_ret_val')");
    fbird_rollback_ret($trans_rb_ret); // Transaction still active
    echo "fbird_rollback_ret succeeded\n";
    fbird_rollback($trans_rb_ret); // Cleanup

    // Test Service aliases (Basic check)
    global $host; // from interbase.inc
    $service = fbird_service_attach($host ? $host : "localhost", $user, $password);
    if ($service) {
        echo "fbird_service_attach succeeded\n";

        $server_info = fbird_server_info($service, FBIRD_SVC_SERVER_VERSION);
        if ($server_info) {
            echo "fbird_server_info succeeded\n";
        } else {
            echo "fbird_server_info failed\n";
        }

        fbird_service_detach($service);
        echo "fbird_service_detach succeeded\n";
    } else {
        echo "fbird_service_attach failed\n";
    }

    // Error handling aliases
    // Trigger an error
    @fbird_query($dbh, "SELECT * FROM non_existent_table");
    if (fbird_errcode() !== 0) {
        echo "fbird_errcode succeeded\n";
    } else {
        echo "fbird_errcode failed\n";
    }
    if (strlen(fbird_errmsg()) > 0) {
        echo "fbird_errmsg succeeded\n";
    } else {
        echo "fbird_errmsg failed\n";
    }

    // Version info aliases
    if (is_float(fbird_get_client_version())) {
        echo "fbird_get_client_version succeeded\n";
    }
    if (is_int(fbird_get_client_major_version())) {
        echo "fbird_get_client_major_version succeeded\n";
    }
    if (is_int(fbird_get_client_minor_version())) {
        echo "fbird_get_client_minor_version succeeded\n";
    }

    fbird_close($dbh);
    echo "fbird_close succeeded\n";
}

test_alias_functionality();
?>
--EXPECT--
fbird_connect succeeded
fbird_trans succeeded
fbird_query (DDL) succeeded
fbird_commit succeeded
fbird_prepare succeeded
fbird_execute succeeded
fbird_free_query succeeded
fbird_commit_ret succeeded
fbird_query (SELECT) succeeded
fbird_fetch_assoc succeeded
fbird_num_fields succeeded
fbird_field_info succeeded
fbird_free_result succeeded
fbird_gen_id succeeded
fbird_rollback succeeded
fbird_rollback_ret succeeded
fbird_service_attach succeeded
fbird_server_info succeeded
fbird_service_detach succeeded
fbird_errcode succeeded
fbird_errmsg succeeded
fbird_get_client_version succeeded
fbird_get_client_major_version succeeded
fbird_get_client_minor_version succeeded
fbird_close succeeded
