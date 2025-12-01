# BabySteps Implementation Plan: PHP 8.1 Test Fixes

**Created**: 2025-11-30
**Target**: Fix all 9 failing tests in php81-dev environment
**Methodology**: Each step is atomic, testable, and reversible

---

## Overview

This document provides exact implementation steps following the BabySteps methodology:
1. One atomic change per step
2. Verify after each step
3. Commit after each successful verification
4. Rollback if verification fails

---

## Phase 1: Test Expectation Fixes (Low Risk)

### Step 1.1: Review test DDL for ibase_field_info tests

**Action**: Read the SQL table creation in test files to confirm field types

```bash
grep -A 20 "CREATE TABLE" tests/ibase_field_info_001.phpt
grep -A 20 "CREATE TABLE" tests/ibase_field_info_004.phpt
```

**Expected Finding**: Fields named `CHAR_*` are declared as `CHAR`, not `VARCHAR`

**Verification**: Visual confirmation of DDL statements

---

### Step 1.2: Fix tests/ibase_field_info_001.phpt expectations

**Action**: Replace `'VARCHAR'` with `'CHAR'` for fields that are actually CHAR type

**File**: `tests/ibase_field_info_001.phpt`

**Search for this block in --EXPECT-- section**:
```php
    'type' => 'VARCHAR',
```

**Replace with** (for CHAR fields only):
```php
    'type' => 'CHAR',
```

**Specific fields to check**:
- `CHAR_FIXED` → should be `'CHAR'`
- `CHAR_UTF8` → should be `'CHAR'`
- `BINARY_FIXED` → should be `'CHAR'`

**Verification**:
```bash
docker exec php-firebird-dev-php81-dev-1 php /ext/run-tests.php /ext/tests/ibase_field_info_001.phpt
```

**Success Criteria**: Test passes

---

### Step 1.3: Fix tests/ibase_field_info_004.phpt expectations

**Action**: Same as Step 1.2 but for ibase_field_info_004.phpt

**File**: `tests/ibase_field_info_004.phpt`

**Apply same CHAR/VARCHAR corrections**

**Verification**:
```bash
docker exec php-firebird-dev-php81-dev-1 php /ext/run-tests.php /ext/tests/ibase_field_info_004.phpt
```

**Success Criteria**: Test passes

---

### Step 1.4: Fix tests/savepoint_001.phpt whitespace pattern

**Action**: Update expected output to remove problematic pattern match

**File**: `tests/savepoint_001.phpt`

**Current (problematic)**:
```
--EXPECTF--
bool(true)%a
```

**Replace with**:
```
--EXPECT--
bool(true)
```

**Verification**:
```bash
docker exec php-firebird-dev-php81-dev-1 php /ext/run-tests.php /ext/tests/savepoint_001.phpt
```

**Success Criteria**: Test passes

---

### Step 1.5: Commit Phase 1 changes

**Action**: 
```bash
git add tests/ibase_field_info_001.phpt tests/ibase_field_info_004.phpt tests/savepoint_001.phpt
git commit -m "fix(tests): correct CHAR/VARCHAR type expectations and whitespace pattern

- ibase_field_info_001.phpt: CHAR fields now expect 'CHAR' type not 'VARCHAR'
- ibase_field_info_004.phpt: Same CHAR type corrections
- savepoint_001.phpt: Remove problematic %a pattern, use --EXPECT--

Firebird correctly distinguishes CHAR (fixed-length) from VARCHAR (variable-length).
The extension was correct; test expectations were wrong."
```

**Verification**:
```bash
./scripts/host/test_matrix.sh php81-dev 2>&1 | grep -E "(PASS|FAIL).*field_info|savepoint"
```

**Success Criteria**: 3 additional tests now pass

---

## Phase 2: Cursor State Management (Medium Risk)

### Step 2.1: Add cursor error suppression code 335544573

**Action**: Add `isc_dsql_cursor_close_err` (335544573) to suppressed errors

**File**: `ibase_query_exec.c`

**Location**: Find the cursor close error handling (around line 890-910)

**Current code pattern**:
```c
if (IB_STATUS[1] == 335544569 || IB_STATUS[1] == 335544436) {
    suppress_error = 1;
}
```

**Add 335544573**:
```c
if (IB_STATUS[1] == 335544569 || IB_STATUS[1] == 335544436 || IB_STATUS[1] == 335544573) {
    suppress_error = 1;
}
```

**Verification**:
```bash
docker exec php-firebird-dev-php81-dev-1 php /ext/run-tests.php /ext/tests/proc-001.phpt
```

**Success Criteria**: No "Attempt to reclose a closed cursor" warning

---

### Step 2.2: Add is_child_result field to ibase_query struct

**Action**: Add boolean field to track child vs parent relationships

**File**: `php_ibase_query_internal.h`

**Find ibase_query struct and add field**:
```c
typedef struct {
    // ... existing fields ...
    unsigned int is_child_result;  // 1 if this is a result from execute, not prepare
} ibase_query;
```

**Verification**: Compile without errors
```bash
docker exec php-firebird-dev-php81-dev-1 bash -c "cd /ext && phpize --clean && phpize && ./configure && make clean && make"
```

**Success Criteria**: Successful compilation

---

### Step 2.3: Set is_child_result=1 when creating result resource

**Action**: Mark child resources in _php_ibase_exec()

**File**: `ibase_query_exec.c`

**Location**: In `_php_ibase_exec()`, where `result_query` is created (around line 950-980)

**Find this pattern**:
```c
result_query = emalloc(sizeof(ibase_query));
memset(result_query, 0, sizeof(ibase_query));
```

**Add after**:
```c
result_query->is_child_result = 1;
```

**Also ensure parent is marked**:
```c
ib_query->is_child_result = 0;  // Parent is not a child
```

**Verification**: Compile without errors

---

### Step 2.4: Modify php_ibase_free_query_rsrc() for child handling

**Action**: Child resources should not free parent's statement

**File**: `ibase_query_exec.c`

**Location**: `php_ibase_free_query_rsrc()` function (around line 614-678)

**Find the function and modify**:
```c
static void php_ibase_free_query_rsrc(zend_resource *rsrc)
{
    ibase_query *ib_query = (ibase_query *)rsrc->ptr;
    
    if (ib_query == NULL) {
        return;
    }
    
    if (ib_query->is_child_result) {
        // Child result: only reset local state, don't touch shared statement
        ib_query->is_open = 0;
        ib_query->has_more_rows = 0;
        
        // Free only the child's own allocations
        if (ib_query->out_sqlda) {
            _php_ibase_free_xsqlda(ib_query->out_sqlda);
            ib_query->out_sqlda = NULL;
        }
        
        // Free the child structure itself
        efree(ib_query);
    } else {
        // Parent prepared statement: full cleanup
        _php_ibase_free_query(ib_query);
    }
}
```

**Verification**:
```bash
docker exec php-firebird-dev-php81-dev-1 php /ext/run-tests.php /ext/tests/006.phpt
```

**Success Criteria**: Test 006.phpt passes

---

### Step 2.5: Verify all Group 1 tests pass

**Action**: Run all affected tests

**Verification**:
```bash
docker exec php-firebird-dev-php81-dev-1 php /ext/run-tests.php /ext/tests/006.phpt /ext/tests/bug45373.phpt /ext/tests/repro_var_export_bug.phpt
```

**Success Criteria**: All 3 tests pass

---

### Step 2.6: Commit Phase 2 changes

**Action**:
```bash
git add ibase_query_exec.c php_ibase_query_internal.h
git commit -m "fix(query): proper parent-child resource lifecycle management

- Add is_child_result field to ibase_query struct
- Child result destructors no longer free parent's statement handle  
- Add 335544573 (isc_dsql_cursor_close_err) to suppressed errors
- Fixes: 006.phpt, bug45373.phpt, repro_var_export_bug.phpt, proc-001.phpt

Root cause: ibase_free_result() on child was calling isc_dsql_free_statement
which invalidated the parent prepared query's cursor state."
```

---

## Phase 3: Array Segfault Fix (High Risk)

### Step 3.1: Run valgrind on 007.phpt to get stack trace

**Action**: Capture exact crash location

```bash
docker exec php-firebird-dev-php81-dev-1 bash -c "cd /ext/tests && valgrind --tool=memcheck --track-origins=yes php 007.php 2>&1" | head -100
```

**Expected Output**: Stack trace showing `_php_ibase_bind_array()` with buffer overflow

**Document the output for analysis**

---

### Step 3.2: Add bounds check at start of _php_ibase_bind_array()

**Action**: Validate buf_size before processing

**File**: `ibase_query_exec.c`

**Location**: Start of `_php_ibase_bind_array()` function (around line 715)

**Add after variable declarations**:
```c
// Validate buffer size is sane
if (buf_size == 0 || buf_size > (size_t)ib_array->ar_size) {
    _php_ibase_module_error("Invalid array buffer size: %zu (expected max: %ld)", 
                            buf_size, (long)ib_array->ar_size);
    return FAILURE;
}
```

**Verification**: Compile and run test (may still fail but with better error)

---

### Step 3.3: Fix SQL_VARYING buffer size calculation

**Action**: Use correct element size for varying types

**File**: `ibase_query_exec.c`

**Location**: SQL_VARYING case in `_php_ibase_bind_array()` (around line 759-766)

**Current code**:
```c
case SQL_VARYING:
    convert_to_string(val);
    size_t str_len = Z_STRLEN_P(val);
    size_t max_len = buf_size - sizeof(short);
```

**Replace with**:
```c
case SQL_VARYING:
    convert_to_string(val);
    size_t str_len = Z_STRLEN_P(val);
    
    // For VARYING, element size = declared_length + sizeof(short)
    // buf_size here is the SLICE size, which should be el_size for leaf nodes
    // max_len is the data portion only (excluding length prefix)
    size_t el_size = ib_array->ar_desc.array_desc_length + sizeof(short);
    size_t max_len = ib_array->ar_desc.array_desc_length;  // Just data, not prefix
    
    // Bounds check
    if (buf_size < el_size) {
        _php_ibase_module_error("Array element buffer too small: %zu < %zu", buf_size, el_size);
        return FAILURE;
    }
```

**Verification**:
```bash
docker exec php-firebird-dev-php81-dev-1 php /ext/run-tests.php /ext/tests/007.phpt
```

---

### Step 3.4: Verify slice_size calculation in recursive calls

**Action**: Ensure slice_size correctly divides for multi-dimensional arrays

**File**: `ibase_query_exec.c`

**Location**: Recursive call in `_php_ibase_bind_array()` (around line 730-735)

**Current pattern**:
```c
size_t slice_size = buf_size / dim_len;
if (_php_ibase_bind_array(slice, buf, slice_size, ib_array, dim + 1) == FAILURE) {
```

**Add validation**:
```c
size_t slice_size = buf_size / dim_len;

// Validate slice size is sufficient for remaining dimensions
if (dim + 1 == ib_array->ar_desc.array_desc_dimensions) {
    // Leaf dimension: slice_size must equal element size
    size_t expected_el_size;
    if (ib_array->ar_desc.array_desc_dtype == blr_varying || 
        ib_array->ar_desc.array_desc_dtype == blr_varying2) {
        expected_el_size = ib_array->ar_desc.array_desc_length + sizeof(short);
    } else {
        expected_el_size = ib_array->ar_desc.array_desc_length;
    }
    
    if (slice_size < expected_el_size) {
        _php_ibase_module_error("Array slice too small at dimension %d: %zu < %zu", 
                                dim + 1, slice_size, expected_el_size);
        return FAILURE;
    }
}

if (_php_ibase_bind_array(slice, buf, slice_size, ib_array, dim + 1) == FAILURE) {
```

**Verification**:
```bash
docker exec php-firebird-dev-php81-dev-1 valgrind --tool=memcheck php /ext/tests/007.php 2>&1 | grep -E "(ERROR|Invalid)"
```

**Success Criteria**: No valgrind errors

---

### Step 3.5: Run full 007.phpt test

**Verification**:
```bash
docker exec php-firebird-dev-php81-dev-1 php /ext/run-tests.php /ext/tests/007.phpt
```

**Success Criteria**: Test passes without segfault

---

### Step 3.6: Commit Phase 3 changes

**Action**:
```bash
git add ibase_query_exec.c
git commit -m "fix(array): prevent buffer overflow in multi-dimensional VARCHAR arrays

- Add bounds checking at _php_ibase_bind_array() entry
- Fix SQL_VARYING element size calculation to use array_desc_length 
- Validate slice_size at each recursion level
- For blr_varying/blr_varying2: el_size = desc_length + sizeof(short)

Root cause: buf_size was passed incorrectly through recursion without 
accounting for the 2-byte length prefix that VARCHAR elements require.
This caused memcpy to write past buffer boundaries in deep recursion."
```

---

## Phase 4: fbird_list_table_blockers Fix (Medium Risk)

### Step 4.1: Add status check before _php_ibase_error()

**Action**: Only report error when status[1] != 0

**File**: `ibase_inspection.c`

**Location**: Error handling in `fbird_list_table_blockers()` (around line 120-130)

**Find this pattern**:
```c
if (isc_dsql_fetch(status, (isc_stmt_handle*)&stmt, 1, out_sqlda)) {
    if (status[1] == 100) break; // EOF
    _php_ibase_error();
```

**Replace with**:
```c
if (isc_dsql_fetch(status, (isc_stmt_handle*)&stmt, 1, out_sqlda)) {
    if (status[1] == 100) break; // EOF
    if (status[1] != 0) {
        _php_ibase_error();
    }
    // status[1] == 0 means successful fetch but no more rows - treat as EOF
    break;
```

**Verification**:
```bash
docker exec php-firebird-dev-php81-dev-1 php /ext/run-tests.php /ext/tests/migration_001.phpt
```

---

### Step 4.2: Handle empty result set gracefully

**Action**: Return empty array for no blockers found

**File**: `ibase_inspection.c`

**The function should already initialize return_value as array. Verify**:
```c
array_init(return_value);
// ... fetch loop ...
// On EOF or no rows: return_value is already an empty array - correct behavior
```

**Verification**:
```bash
docker exec php-firebird-dev-php81-dev-1 php -r "
\$db = ibase_connect('/firebird/data/test.fdb', 'SYSDBA', 'masterkey');
\$result = fbird_list_table_blockers(\$db, 'NONEXISTENT_TABLE');
var_dump(\$result);
ibase_close(\$db);
"
```

**Expected Output**: `array(0) {}` (empty array, not false)

---

### Step 4.3: Commit Phase 4 changes

**Action**:
```bash
git add ibase_inspection.c
git commit -m "fix(inspection): handle empty results in fbird_list_table_blockers

- Check status[1] != 0 before calling _php_ibase_error()
- status[1] == 0 after fetch means EOF, not error
- Return empty array when no blockers found

Root cause: isc_dsql_fetch returning with status[1]==0 was incorrectly
treated as error, causing 'unknown ISC error 0' warning."
```

---

## Phase 5: Final Verification

### Step 5.1: Run full test matrix

**Action**:
```bash
./scripts/host/test_matrix.sh php81-dev 2>&1 | tee test_results_after_fixes.log
```

**Success Criteria**: All 9 previously failing tests now pass

---

### Step 5.2: Run valgrind check on all fixed tests

**Action**:
```bash
for test in 006 007 proc-001 migration_001; do
    echo "=== Testing $test ==="
    docker exec php-firebird-dev-php81-dev-1 valgrind --tool=memcheck php /ext/tests/$test.php 2>&1 | grep -E "(ERROR|definitely lost|Invalid)"
done
```

**Success Criteria**: No memory errors reported

---

### Step 5.3: Document changes

**Action**: Update CHANGELOG and documentation

```bash
git add docs/development/TEST_FAILURE_ANALYSIS_PHP81.md docs/development/BABYSTEPS_IMPLEMENTATION_PLAN.md
git commit -m "docs: add test failure analysis and implementation plan

- Documented 6 issue groups with root causes
- Created BabySteps implementation plan with 20+ atomic steps  
- Added research findings on PHP 8.x resources, Firebird XSQLDA, debugging
- Included Firebird SQL type reference table"
```

---

## Rollback Procedures

If any step fails verification:

### Rollback Single File
```bash
git checkout HEAD -- <filename>
```

### Rollback Entire Phase
```bash
git reset --hard HEAD~N  # where N is number of commits in phase
```

### Emergency Full Rollback
```bash
git log --oneline -10  # Find commit before changes
git reset --hard <commit-hash>
```

---

## Success Metrics

| Metric | Before | Target |
|--------|--------|--------|
| Passing Tests | 76/98 (77.6%) | 85/98 (86.7%) |
| Failing Tests | 9 | 0 |
| Segfaults | 1 | 0 |
| Valgrind Errors | Unknown | 0 |

---

## Total Steps: 20

| Phase | Steps | Risk | Tests Fixed |
|-------|-------|------|-------------|
| Phase 1 | 5 | Low | 3 |
| Phase 2 | 6 | Medium | 4 |
| Phase 3 | 6 | High | 1 |
| Phase 4 | 3 | Medium | 1 |
| Phase 5 | 3 | N/A | Verification |
