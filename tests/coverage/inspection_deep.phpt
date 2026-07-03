--TEST--
Inspection Functions - Error Paths and Edge Cases
--EXTENSIONS--
firebird
--SKIPIF--
<?php
// Include firebird test helpers
require __DIR__ . '/../skipif.inc';
// fbird_kill_attachment() with PHP_INT_MAX blocks indefinitely when the
// Firebird server runs in a container or on a remote host (issue #261).
// Skip whenever FIREBIRD_HOST is set (indicates remote/container topology).
if (getenv('FIREBIRD_HOST')) {
    die('skip fbird_kill_attachment blocks on remote/containerized servers (issue #261)');
}
?>
--FILE--
<?php
/**
 * Test comprehensive error paths for fbird_inspection.c
 *
 * Coverage targets:
 * - fbird_kill_attachment(): Invalid IDs, OO API requirement checks
 * - fbird_list_table_blockers(): Non-existent tables, OO API requirement checks
 * - fbird_drop_table_force(): DDL execution, error handling
 */

echo "=== Inspection Error Tests ===\n\n";

require_once __DIR__ . '/../firebird.inc';

// firebird.inc provides $test_base, $user, $password (via config.inc),
// creates the test database via init_db(), and registers cleanup_db().

$conn = fbird_connect($test_base, $user, $password);

if (!$conn) {
    die("SKIP: Could not connect to Firebird\n");
}

echo "Connected to Firebird\n\n";

// Test 1: fbird_kill_attachment() - Invalid attachment ID (non-existent)
echo "1. fbird_kill_attachment() with non-existent attachment ID:\n";
$result = fbird_kill_attachment($conn, PHP_INT_MAX);
if ($result === false || $result === true) {
    echo "   PASS - Returned result for non-existent attachment\n";
} else {
    echo "   FAIL - Should return boolean\n";
}
echo "\n";

// Test 2: fbird_kill_attachment() - Invalid resource
echo "2. fbird_kill_attachment() with invalid resource:\n";
try {
    fbird_kill_attachment(new stdClass(), 123);
    echo "   FAIL - Should throw error\n";
} catch (Throwable $e) {
    echo "   PASS - Error thrown: " . substr($e->getMessage(), 0, 50) . "...\n";
}
echo "\n";

// Test 3: fbird_kill_attachment() - Using transaction resource
echo "3. fbird_kill_attachment() with transaction resource:\n";
$trans = fbird_trans($conn);
if ($trans) {
    $result = fbird_kill_attachment($trans, PHP_INT_MAX);
    if ($result === false || $result === true) {
        echo "   PASS - Returned result for non-existent attachment\n";
    } else {
        echo "   FAIL - Should return boolean\n";
    }
    fbird_commit($trans);
} else {
    echo "   SKIP - Could not create transaction\n";
}
echo "\n";

// Test 4: fbird_list_table_blockers() - Non-existent table
echo "4. fbird_list_table_blockers() with non-existent table:\n";
$result = fbird_list_table_blockers($conn, 'NONEXISTENT_TABLE_12345');
if (is_array($result) && count($result) === 0) {
    echo "   PASS - Returned empty array for non-existent table\n";
} else {
    echo "   Result: " . var_export($result, true) . "\n";
    echo "   Expected empty array\n";
}
echo "\n";

// Test 5: fbird_list_table_blockers() - Invalid resource
echo "5. fbird_list_table_blockers() with invalid resource:\n";
try {
    fbird_list_table_blockers(new stdClass(), 'TABLE1');
    echo "   FAIL - Should throw error\n";
} catch (Throwable $e) {
    echo "   PASS - Error thrown: " . substr($e->getMessage(), 0, 50) . "...\n";
}
echo "\n";

// Test 6: fbird_list_table_blockers() - Using transaction resource
echo "6. fbird_list_table_blockers() with transaction resource:\n";
$trans = fbird_trans($conn);
if ($trans) {
    $result = fbird_list_table_blockers($trans, 'MON$ATTACHMENTS');
    if (is_array($result)) {
        echo "   PASS - Returned array (count: " . count($result) . ")\n";
    } else {
        echo "   FAIL - Should return array\n";
    }
    fbird_commit($trans);
} else {
    echo "   SKIP - Could not create transaction\n";
}
echo "\n";

// Test 7: fbird_drop_table_force() - Drop non-existent table
echo "7. fbird_drop_table_force() with non-existent table:\n";
$result = fbird_drop_table_force($conn, 'NONEXISTENT_TABLE_98765');
if ($result === false) {
    echo "   PASS - Returned false for non-existent table\n";
} else {
    echo "   FAIL - Should return false\n";
}
echo "\n";

// Test 8: fbird_drop_table_force() - Invalid resource
echo "8. fbird_drop_table_force() with invalid resource:\n";
try {
    fbird_drop_table_force(new stdClass(), 'TABLE1');
    echo "   FAIL - Should throw error\n";
} catch (Throwable $e) {
    echo "   PASS - Error thrown: " . substr($e->getMessage(), 0, 50) . "...\n";
}
echo "\n";

// Test 9: fbird_drop_table_force() - Create and drop a test table
echo "9. fbird_drop_table_force() - Create and drop test table:\n";

$trans = fbird_trans($conn);
if ($trans) {
    // Create a test table
    $create_sql = "CREATE TABLE TEST_DROP_TABLE (ID INTEGER PRIMARY KEY, NAME VARCHAR(50))";
    $stmt = fbird_query($trans, $create_sql);
    
    if ($stmt === true || $stmt instanceof \Firebird\Statement || is_resource($stmt)) {
        if ($stmt instanceof \Firebird\Statement || is_resource($stmt)) fbird_free_query($stmt);
        fbird_commit($trans);
        
        // Verify table exists
        $trans2 = fbird_trans($conn);
        if ($trans2) {
            $check_sql = "SELECT COUNT(*) FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'TEST_DROP_TABLE'";
            $result = fbird_query($trans2, $check_sql);
            if ($result instanceof \Firebird\ResultSet || is_resource($result)) {
                $row = fbird_fetch_row($result);
                fbird_free_query($result);
                fbird_commit($trans2);
                
                echo "   Table exists before drop: " . ($row[0] > 0 ? "Yes" : "No") . "\n";
                
                // Drop the table using force
                $trans3 = fbird_trans($conn);
                if ($trans3) {
                    $drop_result = fbird_drop_table_force($trans3, 'TEST_DROP_TABLE');
                    
                    if ($drop_result === true) {
                        echo "   PASS - Table dropped successfully\n";
                    } else {
                        echo "   FAIL - Should return true\n";
                    }
                    
                    // Verify table is gone
                    // fbird_drop_table_force already commits and frees the transaction
                    // fbird_commit($trans3);
                    $trans4 = fbird_trans($conn);
                    if ($trans4) {
                        $check_sql2 = "SELECT COUNT(*) FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'TEST_DROP_TABLE'";
                        $result2 = fbird_query($trans4, $check_sql2);
                        if ($result2 instanceof \Firebird\ResultSet || is_resource($result2)) {
                            $row2 = fbird_fetch_row($result2);
                            fbird_free_query($result2);
                            fbird_commit($trans4);
                            
                            if ($row2[0] == 0) {
                                echo "   PASS - Table no longer exists\n";
                            } else {
                                echo "   FAIL - Table still exists\n";
                            }
                        }
                    }
                }
            }
        }
    } else {
        echo "   FAIL - Could not create test table\n";
        fbird_rollback($trans);
    }
} else {
    echo "   SKIP - Could not create transaction\n";
}
echo "\n";

// Test 10: fbird_drop_table_force() - Using transaction resource
echo "10. fbird_drop_table_force() with transaction resource:\n";

$trans = fbird_trans($conn);
if ($trans) {
    // Create another test table
    $create_sql = "CREATE TABLE TEST_DROP_TABLE2 (ID INTEGER)";
    $stmt = fbird_query($trans, $create_sql);
    
    if ($stmt === true || $stmt instanceof \Firebird\Statement || is_resource($stmt)) {
        if ($stmt instanceof \Firebird\Statement || is_resource($stmt)) fbird_free_query($stmt);
        fbird_commit($trans);
        
        // Drop with transaction resource
        $trans2 = fbird_trans($conn);
        if ($trans2) {
            $drop_result = fbird_drop_table_force($trans2, 'TEST_DROP_TABLE2');
            
            if ($drop_result === true) {
                echo "   PASS - Dropped table with transaction resource\n";
            } else {
                echo "   FAIL - Should return true\n";
            }
            
            // fbird_drop_table_force already commits and frees the transaction
            // fbird_commit($trans2);
        }
    } else {
        echo "   FAIL - Could not create test table\n";
        fbird_rollback($trans);
    }
} else {
    echo "   SKIP - Could not create transaction\n";
}
echo "\n";

// Test 11: fbird_list_table_blockers() - Query system tables directly
echo "11. fbird_list_table_blockers() on MON\$ATTACHMENTS:\n";
$result = fbird_list_table_blockers($conn, 'MON$ATTACHMENTS');
if (is_array($result)) {
    echo "   PASS - Returned array (count: " . count($result) . ")\n";
    if (count($result) > 0) {
        echo "   First blocker: " . var_export($result[0], true) . "\n";
    }
} else {
    echo "   FAIL - Should return array\n";
}
echo "\n";

// Test 12: fbird_list_table_blockers() - Empty table name
echo "12. fbird_list_table_blockers() with empty table name:\n";
$result = fbird_list_table_blockers($conn, '');
if (is_array($result)) {
    echo "   PASS - Returned array for empty table name\n";
} else {
    echo "   FAIL - Should return array\n";
}
echo "\n";

// Test 13: fbird_kill_attachment() - Negative attachment ID
echo "13. fbird_kill_attachment() with negative attachment ID:\n";
$result = fbird_kill_attachment($conn, -1);
if ($result === false || $result === true) {
    echo "   PASS - Returned result for negative ID\n";
} else {
    echo "   FAIL - Should return boolean\n";
}
echo "\n";

// Test 14: fbird_kill_attachment() - Zero attachment ID
echo "14. fbird_kill_attachment() with zero attachment ID:\n";
$result = fbird_kill_attachment($conn, 0);
if ($result === false || $result === true) {
    echo "   PASS - Returned result for zero ID\n";
} else {
    echo "   FAIL - Should return boolean\n";
}
echo "\n";

// Test 15: fbird_drop_table_force() - Table with special characters
echo "15. fbird_drop_table_force() with table name containing special chars:\n";

$trans = fbird_trans($conn);
if ($trans) {
    // Note: Most special characters require quoting, but this tests the function's handling
    $drop_result = fbird_drop_table_force($trans, 'TABLE_WITH_UNDERSCORES');
    
    // This should fail (table doesn't exist), but we're testing error handling
    if ($drop_result === false) {
        echo "   PASS - Returned false for non-existent table\n";
    } else {
        echo "   Result: " . var_export($drop_result, true) . "\n";
    }
    
    fbird_rollback($trans);
} else {
    echo "   SKIP - Could not create transaction\n";
}
echo "\n";

// Cleanup
fbird_close($conn);

echo "=== All Inspection Error Tests Complete ===\n";
?>
--EXPECTF--
=== Inspection Error Tests ===

Connected to Firebird

1. fbird_kill_attachment() with non-existent attachment ID:
   PASS - Returned result for non-existent attachment

2. fbird_kill_attachment() with invalid resource:
   PASS - Error thrown: %A

3. fbird_kill_attachment() with transaction resource:
   PASS - Returned result for non-existent attachment

4. fbird_list_table_blockers() with non-existent table:
   PASS - Returned empty array for non-existent table

5. fbird_list_table_blockers() with invalid resource:
   PASS - Error thrown: %A

6. fbird_list_table_blockers() with transaction resource:
   PASS - Returned array (count: %d)

7. fbird_drop_table_force() with non-existent table:
%A   PASS - Returned false for non-existent table

8. fbird_drop_table_force() with invalid resource:
   PASS - Error thrown: %A

9. fbird_drop_table_force() - Create and drop test table:
   Table exists before drop: Yes
   PASS - Table dropped successfully
   PASS - Table no longer exists

10. fbird_drop_table_force() with transaction resource:
   PASS - Dropped table with transaction resource

11. fbird_list_table_blockers() on MON$ATTACHMENTS:
   PASS - Returned array (count: %d)%A

12. fbird_list_table_blockers() with empty table name:
   PASS - Returned array for empty table name

13. fbird_kill_attachment() with negative attachment ID:
   PASS - Returned result for negative ID

14. fbird_kill_attachment() with zero attachment ID:
   PASS - Returned result for zero ID

15. fbird_drop_table_force() with table name containing special chars:
%A   PASS - Returned false for non-existent table

=== All Inspection Error Tests Complete ===
--CLEAN--
<?php require_once __DIR__ . '/../clean.inc'; ?>
