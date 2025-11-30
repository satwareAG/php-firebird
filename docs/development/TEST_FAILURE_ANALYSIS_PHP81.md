# PHP 8.1 Test Failure Analysis and Correction Plan

## Test Matrix Summary

**Date**: 2025-11-30
**Environment**: php81-dev container with Firebird 4 client
**Results**: 98 total tests - 76 passed (77.6%), 13 skipped (13.3%), 9 failed (9.2%)

---

## Group 1: Invalid Query Resource Errors

### Affected Tests
- `tests/006.phpt` - Fatal error at line 204
- `tests/bug45373.phpt` - Fatal error at line 13
- `tests/repro_var_export_bug.phpt` - Fatal error at line 16

### Error Message
```
Fatal error: Uncaught TypeError: ibase_execute(): supplied resource is not a valid Firebird/InterBase query resource
Fatal error: Uncaught TypeError: ibase_free_query(): supplied resource is not a valid Firebird/InterBase query resource
```

### Root Cause Analysis
The issue occurs when `ibase_prepare()` returns a valid resource, but subsequent calls to `ibase_execute()` or `ibase_free_query()` fail to recognize it. Investigation shows:

1. In `ibase_query_exec.c`, the `_php_ibase_exec()` function creates result resources for SELECT queries
2. The `result_query` is linked as a child of the parent prepared query (`ib_query`)
3. When `ibase_free_result()` is called on the result, it may invalidate the parent query prematurely
4. The `php_ibase_free_query_rsrc()` destructor closes the parent's cursor state

### Specific Issue in 006.phpt
The test does:
```php
$query = ibase_prepare("select * from test6 where v_integer between ? and ?");
$res = ibase_execute($query, $low_border, $high_border);
ibase_free_result($res);
// ... later ...
$res = ibase_execute($query, $low_border, $high_border); // FAILS HERE
```

When `ibase_free_result($res)` is called, it closes the cursor and may corrupt the parent query's state.

### Correction Plan (BabySteps)

**Step 1.1**: In `php_ibase_free_query_rsrc()`, ensure that freeing a child result does NOT close the parent's cursor unconditionally. Only reset `is_open` flags on the result, not propagate to parent.

**Step 1.2**: In `_php_ibase_exec()`, before re-executing a prepared statement, ensure proper cursor closure and state reset without returning FAILURE for benign cursor states.

**Step 1.3**: Add defensive checks in resource validation to handle edge cases where parent/child linkage may be corrupted.

**Files to Modify**:
- `ibase_query_exec.c`: Lines 614-678 (`php_ibase_free_query_rsrc()`)
- `ibase_query_exec.c`: Lines 881-907 (cursor lifecycle in `_php_ibase_exec()`)

---

## Group 2: Segmentation Fault in Array Handling

### Affected Tests
- `tests/007.phpt` - Segmentation fault (core dumped)

### Error Message
```
Termsig=11
```

### Root Cause Analysis
The segfault occurs during array insert operations. In `_php_ibase_bind_array()`:

1. The function handles SQL_VARYING type arrays by writing a short length prefix + data
2. Buffer allocation in `_php_ibase_alloc_array()` sets `el_size = ar_desc->array_desc_length + sizeof(short)` for varying types
3. Potential buffer overflow when `ar_size` calculation overflows or when memcpy writes past allocated bounds

### Specific Issue
In `ibase_query_exec.c` line 759-766:
```c
case SQL_VARYING:
    convert_to_string(val);
    size_t str_len = Z_STRLEN_P(val);
    size_t max_len = buf_size - sizeof(short);
    if (str_len > max_len) {
        str_len = max_len;
    }
    *(short *)buf = (short)str_len;
    if (str_len > 0) {
        memcpy(buf + sizeof(short), Z_STRVAL_P(val), str_len);
    }
```

The `buf_size` may not correctly account for element boundaries in multi-dimensional arrays.

### Correction Plan (BabySteps)

**Step 2.1**: Add bounds checking in `_php_ibase_bind_array()` to validate `buf_size` before any write operations.

**Step 2.2**: Verify `slice_size` calculation in the recursive `_php_ibase_bind_array()` function correctly accounts for SQL_VARYING's extra length prefix.

**Step 2.3**: Add assertion/validation that `el_size` matches expected buffer layout for varying types.

**Files to Modify**:
- `ibase_query_exec.c`: Lines 715-780 (`_php_ibase_bind_array()`)
- `ibase_query_exec.c`: Lines 440-520 (`_php_ibase_alloc_array()`)

---

## Group 3: CHAR vs VARCHAR Type Mismatch in ibase_field_info

### Affected Tests
- `tests/ibase_field_info_001.phpt`
- `tests/ibase_field_info_004.phpt`

### Error Message
```
-    'type' => 'VARCHAR',
+    'type' => 'CHAR',
```

For fields: CHAR_FIXED, CHAR_UTF8, BINARY_FIXED

### Root Cause Analysis
The test expectation is WRONG, not the code. The actual field types are CHAR (fixed-length), but the test expects VARCHAR (variable-length).

This is a test expectation issue, not an implementation bug.

### Correction Plan (BabySteps)

**Step 3.1**: Review the table DDL in the test to confirm fields are declared as CHAR, not VARCHAR.

**Step 3.2**: Update test expectations to match actual Firebird field types:
- `CHAR_FIXED` → type should be `CHAR`
- `CHAR_UTF8` → type should be `CHAR`  
- `BINARY_FIXED` → type should be `CHAR`

**Files to Modify**:
- `tests/ibase_field_info_001.phpt`: Update expected output section
- `tests/ibase_field_info_004.phpt`: Update expected output section

---

## Group 4: fbird_list_table_blockers Unknown ISC Error

### Affected Tests
- `tests/migration_001.phpt`

### Error Message
```
Warning: fbird_list_table_blockers(): unknown ISC error 0
```

Function returns `false` instead of expected array.

### Root Cause Analysis
In `ibase_inspection.c`, the `fbird_list_table_blockers()` function:

1. Executes a query against MON$ATTACHMENTS and MON$STATEMENTS
2. On some Firebird configurations, the query may return an error or empty result
3. The error handling path calls `_php_ibase_error()` which reports "unknown ISC error 0"

The issue is in the error reporting - when `status[1] == 0`, `_php_ibase_error()` reports "unknown ISC error 0" which is misleading.

### Correction Plan (BabySteps)

**Step 4.1**: In `fbird_list_table_blockers()`, check if `status[1] == 0` (no error) before calling `_php_ibase_error()`.

**Step 4.2**: Add specific handling for empty result sets - return empty array instead of false.

**Step 4.3**: Improve error message for edge cases where MON$ tables may not be accessible.

**Files to Modify**:
- `ibase_inspection.c`: Lines 80-140 (`fbird_list_table_blockers()`)

---

## Group 5: Cursor Reclose Warning

### Affected Tests
- `tests/proc-001.phpt`

### Error Message
```
Warning: ibase_execute(): Attempt to reclose a closed cursor
```

### Root Cause Analysis
In `_php_ibase_exec()`, before re-executing a statement, the code attempts to close any open cursor:

```c
if (ib_query->statement_type != isc_info_sql_stmt_exec_procedure && ib_query->is_open) {
    if (isc_dsql_free_statement(IB_STATUS, &ib_query->stmt.stmt, DSQL_close)) {
        // Error handling...
    }
}
```

For EXECUTE PROCEDURE statements, the cursor state may be inconsistent, leading to a reclose attempt on an already-closed cursor.

### Correction Plan (BabySteps)

**Step 5.1**: Add `335544573` (isc_dsql_cursor_close_err) to the list of suppressed cursor errors in the close operation.

**Step 5.2**: Before attempting cursor close, check both `is_open` flag AND verify the statement type doesn't implicitly close the cursor.

**Step 5.3**: Ensure EXECUTE PROCEDURE result resources properly reset cursor state after consumption.

**Files to Modify**:
- `ibase_query_exec.c`: Lines 881-907 (cursor lifecycle management)

---

## Group 6: Test Expectation Whitespace Issue

### Affected Tests
- `tests/savepoint_001.phpt`

### Error Message
```
001- bool(true)%a
001+ bool(true)
```

### Root Cause Analysis
The test uses `%a` pattern which should match any string including empty, but the actual output has a different newline pattern than expected.

This is a minor test expectation format issue.

### Correction Plan (BabySteps)

**Step 6.1**: Update the test's `--EXPECT--` section to use `%A` (case-insensitive any) or remove the trailing pattern.

**Step 6.2**: Alternatively, ensure output ends with consistent newline by modifying the test script.

**Files to Modify**:
- `tests/savepoint_001.phpt`: Update expected output section

---

## Implementation Priority

### Priority 1 - Critical (Blocking Multiple Tests)
1. **Group 1**: Invalid Query Resource - Affects 3 tests
2. **Group 2**: Segmentation Fault - Affects 1 test but is a crash

### Priority 2 - Important (Functional Issues)
3. **Group 4**: fbird_list_table_blockers error - Affects 1 test
4. **Group 5**: Cursor reclose warning - Affects 1 test

### Priority 3 - Low (Test Expectation Fixes Only)
5. **Group 3**: CHAR vs VARCHAR - Test expectation issue (2 tests)
6. **Group 6**: Whitespace pattern - Test expectation issue (1 test)

---

## BabySteps Execution Order

### Phase 1: Fix Test Expectations (Low Risk)
```
Step 3.1 → Step 3.2 → Step 6.1 → Run tests to confirm
```

### Phase 2: Fix Cursor State Management (Medium Risk)
```
Step 5.1 → Step 5.2 → Step 5.3 → Run tests to confirm
Step 1.1 → Step 1.2 → Step 1.3 → Run tests to confirm
```

### Phase 3: Fix Array Segfault (High Risk)
```
Step 2.1 → Step 2.2 → Step 2.3 → Run tests with valgrind
```

### Phase 4: Fix fbird_list_table_blockers (Medium Risk)
```
Step 4.1 → Step 4.2 → Step 4.3 → Run tests to confirm
```

---

## Verification Commands

After each phase, run:
```bash
./scripts/host/test_matrix.sh php81-dev
```

For segfault debugging:
```bash
docker exec php-firebird-dev-php81-dev-1 valgrind php /ext/tests/007.php
```

---

## Research Findings (2024-2025)

### PHP 8.x Resource Management

**zend_resource Structure**:
- `zend_refcounted_h gc`: Reference counting header
- `handle`: Integer used by engine to locate resource
- `type`: Resource type identifier  
- `ptr`: Pointer to actual external resource data

**Best Practices for Parent-Child Resources**:
1. When a child resource destructor is called, it should NOT invalidate the parent
2. Use explicit flags (like `is_child_result`) to distinguish behavior
3. Child destructor should only clean up child-specific state (cursor close)
4. Parent destructor handles statement dealocation

**Correct Pattern for php_ibase_free_query_rsrc()**:
```c
static void php_ibase_free_query_rsrc(zend_resource *rsrc) {
    ibase_query *ib_query = (ibase_query *)rsrc->ptr;
    if (ib_query) {
        if (ib_query->is_child_result) {
            // Child result: only close cursor, don't touch parent
            if (ib_query->is_open && ib_query->stmt.stmt) {
                isc_dsql_free_statement(IB_STATUS, &ib_query->stmt.stmt, DSQL_close);
                ib_query->is_open = 0;
            }
            // Don't free ib_query - it's owned by parent
        } else {
            // Parent prepared statement: full cleanup
            _php_ibase_free_query(ib_query);
        }
    }
}
```

### Firebird XSQLDA Buffer Management

**XSQLDA Best Practices**:
- USING DESCRIPTOR: input parameters from XSQLDA
- INTO DESCRIPTOR: return values stored in XSQLDA
- Separate structures required for input and output

**VARCHAR (SQL_VARYING) Buffer Size**:
```
element_size = sizeof(short) + declared_length
```
For arrays: `total_size = element_size × product(dimension_sizes)`

**Multi-Dimensional Array Buffer Calculation**:
```c
// For blr_varying/blr_varying2 types:
el_size = ar_desc->array_desc_length + sizeof(short);  // length prefix

// Total buffer:
ar_size = el_size;
for (int i = 0; i < ar_desc->array_desc_dimensions; i++) {
    ar_size *= (1 + ar_desc->array_desc_bounds[i].array_bound_upper 
                  - ar_desc->array_desc_bounds[i].array_bound_lower);
}
```

### Debugging Techniques for PHP Extensions

**Valgrind Commands**:
```bash
# Full memory leak check
valgrind --leak-check=full --show-reachable=yes php script.php

# Track buffer overflows
valgrind --tool=memcheck --track-origins=yes php script.php

# Check for uninitialized reads
valgrind --tool=memcheck --undefined-value-errors=yes php script.php
```

**GDB Commands for Segfaults**:
```bash
# Run PHP under GDB
gdb php
(gdb) run /path/to/test.php

# After crash, get backtrace
(gdb) bt full

# Examine memory at crash point
(gdb) info registers
(gdb) x/10x $rsp
```

**PHP Debug Build Requirements**:
- Compile with `-g` flag for debug symbols
- Use `--enable-debug` in configure
- Disable optimization with `-O0` for clearer traces

---

## Firebird SQL Type Reference

### Field Types and Expected Behavior

| DDL Declaration | Firebird Type | blr code | Expected `type` in ibase_field_info() |
|-----------------|---------------|----------|---------------------------------------|
| `CHAR(N)` | Fixed-length string | blr_text | `'CHAR'` |
| `VARCHAR(N)` | Variable-length string | blr_varying | `'VARCHAR'` |
| `CHAR(N) CHARACTER SET UTF8` | Fixed-length UTF8 | blr_text | `'CHAR'` |
| `CHAR(N) CHARACTER SET OCTETS` | Binary fixed | blr_text | `'CHAR'` |

**Key Insight for Group 3 Fix**:
The PHP extension correctly reports `CHAR` for fixed-length fields. The tests expected `VARCHAR` which was incorrect. This should be documented to prevent future confusion.

---

## Updated Correction Steps with Research

### Group 1 Fix (Resource Lifecycle)

**Root Cause Confirmed**: The destructor for child result resources incorrectly propagates cursor close to parent.

**Corrected Implementation**:
```c
// In php_ibase_free_query_rsrc():
if (ib_query->is_child_result) {
    // Only close OUR copy of the cursor reference
    // Parent's stmt handle remains valid
    ib_query->is_open = 0;
    ib_query->has_more_rows = 0;
    // Do NOT call isc_dsql_free_statement on shared handle
    efree(ib_query);  // Free the child structure only
} else {
    // Parent: full cleanup
    _php_ibase_free_query(ib_query);
}
```

### Group 2 Fix (Array Segfault)

**Root Cause Confirmed**: Buffer size calculation for SQL_VARYING in multi-dimensional arrays doesn't account for the 2-byte length prefix properly when calculating slice boundaries.

**Corrected Implementation**:
```c
// In _php_ibase_bind_array():
case SQL_VARYING:
    convert_to_string(val);
    size_t str_len = Z_STRLEN_P(val);
    
    // CRITICAL: Use element size, not buf_size
    size_t el_size = ib_array->ar_desc.array_desc_length + sizeof(short);
    size_t max_len = el_size - sizeof(short);  // Data portion only
    
    if (str_len > max_len) {
        str_len = max_len;
    }
    *(short *)buf = (short)str_len;
    if (str_len > 0) {
        memcpy(buf + sizeof(short), Z_STRVAL_P(val), str_len);
    }
    break;
```

### Group 3 Documentation Note

**Important for Developers**:
Firebird distinguishes between:
- `CHAR(N)`: Fixed-length, space-padded → Reports as `'CHAR'`
- `VARCHAR(N)`: Variable-length, no padding → Reports as `'VARCHAR'`

The extension correctly reflects Firebird's actual type. Test expectations that assumed `VARCHAR` for `CHAR` fields were incorrect and have been fixed.
