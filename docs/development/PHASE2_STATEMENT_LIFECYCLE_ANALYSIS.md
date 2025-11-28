# Phase 2: Statement Stability & Lifecycle Analysis

## Overview
This document details the deep analysis and verification performed for Phase 2 of the `php-firebird` improvement roadmap. The focus was on verifying Statement Reuse and Commit Retaining lifecycle behaviors.

## Task 1: Prepared Statement Reuse

### Objective
Verify if `ibase_execute` efficiently reuses prepared statements without re-preparing them, and preserves the handle for multiple executions.

### Analysis
- **Source Code (`ibase_query_exec.c`)**:
  - `ibase_prepare` allocates `ibase_query` and calls `isc_dsql_prepare`.
  - `ibase_execute` retrieves the `ibase_query` resource.
  - `_php_ibase_exec` calls `isc_dsql_execute` (or `execute2`) using the existing `stmt` handle.
  - No re-preparation logic was found in the execution path.
  - For `SELECT` statements, a new *result* resource is created, inheriting the statement handle but explicitly *not* owning it (`owns_stmt_handle = 0`), preventing premature closure.

### Verification
- **Test Script**: `tests/repro_stmt_reuse.phpt`
- **Methodology**: Prepare a statement once, execute it 3 times, fetch results each time.
- **Result**: PASSED. The statement was successfully executed multiple times with the same handle.

### Conclusion
The extension correctly implements prepared statement reuse. No code changes are required for this feature. The architecture properly separates statement preparation from execution resource management.

---

## Task 2: Commit Retaining Resource Lifecycle

### Objective
Confirm if `ibase_commit_ret()` (and rollback retaining) keeps the PHP transaction resource valid and usable, avoiding the need for developers to fetch new transaction handles.

### Analysis
- **Source Code (`interbase.c`)**:
  - `_php_ibase_trans_end` handles transaction completion.
  - Logic includes:
    ```c
    if ((commit & RETAIN) == 0 && res_id != 0) {
        zend_list_delete(Z_RES_P(arg));
    }
    ```
  - This explicitly skips resource deletion if `RETAIN` bit is set.
  - `isc_commit_retaining` is called on the underlying handle.

### Verification
- **Test Script**: `tests/repro_commit_ret_lifecycle.phpt`
- **Methodology**:
  1. Start explicit transaction.
  2. Perform INSERT.
  3. Call `ibase_commit_ret($trans)`.
  4. Perform another INSERT using the *same* `$trans` variable.
  5. Final commit.
  6. Verify both records exist.
- **Result**: PASSED. The second insert succeeded, proving the `$trans` resource remained valid and the underlying Firebird transaction context was preserved.

### Conclusion
The `commit_retaining` lifecycle is correctly implemented. The PHP resource wrapper is preserved, and the underlying Firebird transaction remains active and usable.

---

## Summary
Both targeted improvements for Phase 2 (Statement Reuse and Commit Retaining) were found to be **already implemented and functioning correctly**.

- **Next Steps**:
  - Proceed to **Phase 3** (if planned) or focus on other specific bug fixes.
  - Consider adding these verification tests (`tests/repro_*.phpt`) to the permanent test suite as regression tests.
