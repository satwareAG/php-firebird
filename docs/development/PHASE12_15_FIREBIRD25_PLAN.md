# Phase 12-15: Firebird 2.5 Server Support - Implementation Plan

## Executive Summary

**Goal:** Achieve 80%+ test pass rate on Firebird 2.5 servers using Firebird client >= 3.0 (OO API).

**Current State:** 53/98 tests passing (54.1%), 45 failing (45.9%)
**Target:** 80+ tests passing (80%+)

**Root Cause Identified:** The extension exclusively uses the OO API (Firebird 3.0+ client library) but the parameter binding system still populates legacy XSQLDA structures. These values are never transferred to the OO API message buffers, causing segfaults on parameterized query execution.

---

## Architecture Decision: OO API Only

### Policy Statement

**The php-firebird extension ONLY supports the OO (Object-Oriented) API introduced in Firebird 3.0+ client libraries.** This is a deliberate architectural decision, not a limitation.

### Client vs Server Compatibility

| Component | Version | Notes |
|-----------|---------|-------|
| **Firebird Client Library** | 3.0+ (tested: 4.0.5) | REQUIRED - provides OO API |
| **Firebird Server** | 2.5, 3.0, 4.0, 5.0 | All supported via OO API client |

**Key Insight:** The Firebird 3.0+ client's OO API maintains backward compatibility with older Firebird 2.5 servers. The client negotiates the appropriate wire protocol automatically.

### Why OO API Only?

1. **Simplified Maintenance:** Single code path instead of dual legacy/OO paths
2. **Modern Architecture:** OO API provides cleaner interfaces for attachments, transactions, statements
3. **Resource Management:** Better handle lifecycle management through opaque pointers
4. **Future-Proof:** Legacy `isc_*` API is deprecated and will be removed in future Firebird versions
5. **Consistent Behavior:** Same API semantics across all server versions

### Docker Test Infrastructure

Our Docker test infrastructure demonstrates this architecture:

```yaml
# docker-compose.yml
services:
  php84-dev:
    # Uses Firebird 4.0.5 CLIENT library
    # Connects to firebird25 SERVER
    
  firebird25:
    image: jacobalberty/firebird:2.5-ss
    # Firebird 2.5 server - accessed via FB 4.0.5 client OO API
```

### Legacy API Removal

The following legacy `isc_*` functions are being systematically removed or guarded:
- `isc_dsql_execute_immediate()` → Use OO API statement execution
- `isc_dsql_execute()` / `isc_dsql_execute2()` → Use `fbs_execute()` / `fbs_open_cursor()`
- `isc_start_transaction()` → Use `fbt_start()`
- `isc_commit_transaction()` / `isc_rollback_transaction()` → Use `fbt_commit()` / `fbt_rollback()`
- `isc_dsql_prepare()` → Use `fbs_prepare()`

**Note:** Some legacy functions like `isc_array_get_slice()` may remain temporarily until OO API equivalents are fully implemented.

---

## Priority 1: Fix Critical Segfaults (Blocking ~30 tests)

### Issue 1.1: Transaction SQL Execution (Lines 127-180 in fbird_query_exec.c)

**Problem:** `isc_dsql_execute_immediate()` is called with `&ib_query->link->handle.db` for:
- `isc_info_sql_stmt_start_trans` (SET TRANSACTION)
- `isc_info_sql_stmt_commit` (COMMIT via SQL)
- `isc_info_sql_stmt_rollback` (ROLLBACK via SQL)

For OO API connections, `handle.db` is NULL/invalid, causing segfaults.

**Solution:** Add OO API path before legacy calls:

```c
case isc_info_sql_stmt_commit:
case isc_info_sql_stmt_rollback:
    /* Check if using OO API transaction */
    if (ib_query->trans && ib_query->trans->fbt_transaction) {
        int success;
        if (ib_query->statement_type == isc_info_sql_stmt_commit) {
            success = fbt_commit(ib_query->trans->fbt_transaction, IB_STATUS);
        } else {
            success = fbt_rollback(ib_query->trans->fbt_transaction, IB_STATUS);
        }
        if (!success) {
            _php_fbird_error();
            goto _php_fbird_ex_error;
        }
        /* Mark transaction as closed */
        ib_query->trans->fbt_transaction = NULL;
        ib_query->trans->handle.tr = 0;
        
        if (ib_query->trans_res != NULL) {
            zend_list_delete(ib_query->trans_res);
            ib_query->trans_res = NULL;
        }
        RETVAL_TRUE;
        return SUCCESS;
    }
    /* Fall through to legacy path */
    if (isc_dsql_execute_immediate(...)) { ... }
```

**Files:** `fbird_query_exec.c`
**Tests Fixed:** fbird_trans_008-011.phpt (COMMIT/ROLLBACK via SQL)

### Issue 1.2: SET TRANSACTION via SQL (Line 132)

**Problem:** `isc_dsql_execute_immediate()` with NULL transaction handle on OO connection.

**Solution:** For OO connections, use `fbt_start()` instead:

```c
case isc_info_sql_stmt_start_trans:
    if (ib_query->link->fbc_connection) {
        /* OO API path */
        void* attachment = fbc_get_attachment(ib_query->link->fbc_connection);
        /* Parse TPB from SQL if needed, or use defaults */
        void* oo_trans = fbt_start(IBG(master_instance), attachment, 0, NULL, IB_STATUS);
        if (!oo_trans) {
            _php_fbird_error();
            goto _php_fbird_ex_error;
        }
        /* Register transaction... */
    } else {
        /* Legacy path */
        isc_dsql_execute_immediate(...);
    }
```

**Files:** `fbird_query_exec.c`
**Tests Fixed:** fbird_trans_002-007.phpt, fbird_trans_012-014.phpt

---

## Priority 2: Fix Query Execution Issues (~10 tests)

### Issue 2.1: Legacy Execution Fallback (Lines 295-345)

**Problem:** After OO API prepare succeeds but certain execution paths fall through to legacy `isc_dsql_execute()` which uses invalid handles.

**Current Code (approx line 320):**
```c
/* Fallback to legacy execution */
if (ib_query->statement_type == isc_info_sql_stmt_exec_procedure) { ... }
isc_result = isc_dsql_execute2(IB_STATUS, &ib_query->trans->handle.tr, ...);
```

**Solution:** Ensure all execution paths are covered by OO API code, or guard legacy fallback:

```c
/* Only use legacy execution if we have valid legacy handles */
if (ib_query->stmt.stmt != 0 && ib_query->trans->handle.tr != 0) {
    /* Legacy execution */
    isc_result = isc_dsql_execute2(...);
} else if (ib_query->fbs_statement && ib_query->trans->fbt_transaction) {
    /* Should have been handled by OO API path above - error */
    _php_fbird_module_error("Unexpected OO API execution fallthrough");
    goto _php_fbird_ex_error;
} else {
    _php_fbird_module_error("Invalid query/transaction state");
    goto _php_fbird_ex_error;
}
```

**Files:** `fbird_query_exec.c`

### Issue 2.2: Stored Procedure Execution (isc_info_sql_stmt_exec_procedure)

**Problem:** Stored procedure execution uses legacy `isc_dsql_execute2()`.

**Solution:** Add OO API path for EXECUTE PROCEDURE:

```c
if (ib_query->statement_type == isc_info_sql_stmt_exec_procedure) {
    if (ib_query->fbs_statement && ib_query->trans->fbt_transaction) {
        /* OO API: Use fbs_execute with in/out message buffers */
        oo_api_success = fbs_execute(
            IBG(master_instance),
            ib_query->fbs_statement,
            transaction_ptr,
            ib_query->in_msg_buffer,
            ib_query->in_metadata,
            ib_query->out_msg_buffer,
            ib_query->out_metadata,
            IB_STATUS
        );
        if (!oo_api_success) {
            _php_fbird_error();
            goto _php_fbird_ex_error;
        }
        isc_result = 0;
    } else {
        /* Legacy path... */
    }
}
```

**Files:** `fbird_query_exec.c`
**Tests Fixed:** proc-001.phpt

---

## Priority 3: Fix Array Handling (5 tests)

### Issue 3.1: Legacy Array APIs

**Problem:** `isc_array_get_slice()` and `isc_array_put_slice()` require legacy handles.

**Current Location:** `fbird_query_array.c`

**Solution Options:**

**Option A (Recommended):** Use OO API attachment handle with legacy array functions
- The legacy array functions should work with an attachment handle obtained from OO API
- Need to verify `fbc_get_attachment()` returns a compatible handle

**Option B:** Implement OO API array wrappers
- Much more complex, involves IArray interface
- Defer to Phase 2 (Firebird 3.0 servers)

**Immediate Fix:** Check if OO API attachment is compatible with legacy array calls:

```c
/* In array get/put functions */
isc_db_handle db_handle;
if (link->fbc_connection) {
    /* OO API: get native attachment handle */
    void* attachment = fbc_get_attachment(link->fbc_connection);
    /* The OO API attachment wraps an IAttachment* which has getHandle() */
    db_handle = (isc_db_handle)fbc_get_native_handle(link->fbc_connection);
} else {
    db_handle = link->handle.db;
}
```

**Files:** `fbird_query_array.c`, `firebird_utils.cpp` (add `fbc_get_native_handle`)
**Tests Fixed:** 007.phpt, 007_iso_*.phpt (5 tests)

---

## Priority 4: Fix Metadata Functions (4 tests)

### Issue 4.1: Field Info (fbird_field_info)

**Problem:** Field metadata retrieval may use legacy XSQLDA incorrectly.

**Current:** Uses `ib_query->out_sqlda` which is populated during legacy prepare.

**Solution:** For OO API queries, use `fbm_*` metadata wrapper functions instead of XSQLDA.

**Files:** `fbird_metadata.c`
**Tests Fixed:** fbird_field_info_001.phpt, fbird_field_info_004.phpt

### Issue 4.2: Parameter Info (fbird_param_info)

**Problem:** Similar to field info, parameter metadata may be incorrectly sourced.

**Solution:** For OO API queries, use `fbm_*` input metadata functions.

**Files:** `fbird_metadata.c`
**Tests Fixed:** fbird_param_info_001.phpt, fbird_param_info_004.phpt

---

## Priority 5: Fix Remaining Issues (~15 tests)

### Issue 5.1: RETURNING Clause (returning_001.phpt)

**Problem:** INSERT/UPDATE with RETURNING needs output message buffer handling.

**Solution:** Extend OO API execution to handle `out_sqlda != NULL` case.

### Issue 5.2: Event Handling (008.phpt)

**Problem:** Event handling uses legacy `isc_que_events()`.

**Solution:** Events are already migrated to `fbe_*` wrappers - verify integration.

### Issue 5.3: Savepoints (savepoint_001.phpt)

**Problem:** SAVEPOINT, RELEASE SAVEPOINT, ROLLBACK TO SAVEPOINT use `isc_dsql_execute_immediate()`.

**Solution:** Same pattern as COMMIT/ROLLBACK - add OO API path.

### Issue 5.4: Miscellaneous

- blob_stream_chunked_write.phpt - BLOB stream integration
- bug45373.phpt - Parameter binding error handling
- datatype_char_utf8.phpt - UTF8 character handling
- execute_safety_001.phpt - API safety checks
- fbird_alias_check_002.phpt - Function aliases
- fbird_name_result_001.phpt - Result naming
- migration_001.phpt - Drop table force logic
- repro_segfault_blob.phpt - BLOB cleanup
- repro_var_export_bug.phpt - var_export handling
- use_after_free-002.phpt - Resource cleanup

---

## Implementation Order

### Step 1: Create Mode Detection Header ✅ (Ready to implement)
Create `src/php_fbird_compat.h` with:
- `fbird_api_mode` enum
- `fbird_link_is_oo()`, `fbird_trans_is_oo()`, `fbird_query_is_oo()` helpers
- Guard macros

### Step 2: Fix Transaction SQL (Priority 1)
Fix `case isc_info_sql_stmt_commit/rollback/start_trans` in `fbird_query_exec.c`

**Expected Test Improvement:** +13 tests (fbird_trans_*)

### Step 3: Fix Query Execution Guards (Priority 2)
Add guards around legacy execution fallback

**Expected Test Improvement:** +5-8 tests

### Step 4: Fix Array Handling (Priority 3)
Investigate OO API compatibility with legacy array functions

**Expected Test Improvement:** +5 tests (007*.phpt)

### Step 5: Fix Metadata Functions (Priority 4)
Complete OO API metadata integration

**Expected Test Improvement:** +4 tests

### Step 6: Fix Remaining Issues (Priority 5)
Address miscellaneous failures

**Expected Test Improvement:** +5-10 tests

---

## Success Metrics

| Milestone | Tests Passing | Pass Rate | Status |
|-----------|---------------|-----------|--------|
| Current   | 51/102        | 50%       | ✅ Baseline |
| Step 2    | 64/102        | 63%       | 🎯 Target |
| Step 3    | 72/102        | 71%       | 🎯 Target |
| Step 4    | 77/102        | 76%       | 🎯 Target |
| Step 5    | 81/102        | 79%       | 🎯 Target |
| Step 6    | 85+/102       | 83%+      | 🎯 Final Target |

---

## Files to Modify

1. **NEW:** `src/php_fbird_compat.h` - Mode detection and guards
2. `fbird_query_exec.c` - Transaction SQL and execution paths
3. `fbird_query_array.c` - Array handling compatibility
4. `fbird_metadata.c` - Metadata retrieval
5. `firebird.c` - Connection/transaction mode tagging
6. `firebird_utils.cpp` - Add `fbc_get_native_handle()` if needed

---

## Test Commands

```bash
# Run single test
FIREBIRD_HOST=firebird25 ./scripts/host/test_matrix.sh php84-dev fbird_trans_002

# Run full matrix on firebird25
FIREBIRD_HOST=firebird25 ./scripts/host/test_matrix.sh php84-dev

# Run all PHP versions on firebird25
FIREBIRD_HOST=firebird25 ./scripts/host/test_matrix.sh
```

---

## Phase 2: Firebird 3.0 Server Support (After FB 2.5 stable)

After achieving 80%+ on Firebird 2.5:
1. Run test matrix against `firebird30` container
2. Identify any FB 3.0-specific failures
3. Address protocol/feature differences
4. Target: Same 80%+ pass rate

---

## References

- `implementation_plan.md` - Overall project status
- `docs/development/FIREBIRD_OO_API_REFERENCE.md` - OO API documentation
- `firebird_utils.h` - OO API wrapper declarations
