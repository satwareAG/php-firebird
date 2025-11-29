# Phase 4 Completion Report: Transaction API

**Date:** 2025-11-29
**Status:** Completed
**Scope:** Transaction Parameter Blocks (TPB), Table Reservation, Savepoints, Transaction Inspection

## 1. Objectives Achieved

We have successfully implemented the full scope of the Transaction API RFC, bringing PHP-Firebird's transaction capabilities to parity with the underlying Firebird C-API.

### Feature Table

| Feature | Description | Implementation Details |
| :--- | :--- | :--- |
| **Rich Transaction Start** | `fbird_trans_start($link, $options)` | Replaced opaque default behavior with a full options array parser (`_php_ibase_populate_trans_from_array`) supporting isolation, access mode, lock resolution, and timeouts. |
| **Table Reservation** | Explicit Locking | Implemented the `tables` option array in `fbird_trans_start`, mapping to `isc_tpb_lock_write`, `isc_tpb_lock_read`, etc. |
| **Savepoints** | Native Wrapper | Exposed `fbird_savepoint`, `fbird_rollback_savepoint`, and `fbird_release_savepoint`. |
| **State Inspection** | `fbird_trans_info` | New function to query transaction ID, isolation level, and status from the internal handle. |
| **Buffer Resize** | `TPB_MAX_SIZE` | Increased from 32 bytes (unsafe) to 2048 bytes to accommodate complex TPBs with multiple table names. |

## 2. Verification Strategy

The implementation was verified using a targeted test suite covering happy paths, edge cases, and error conditions.

*   **`tests/trans_tpb_001.phpt`**: Verifies general TPB options (concurrency, read_committed, wait/nowait) and inspection via `fbird_trans_info`.
*   **`tests/trans_tpb_reservation.phpt`**: **NEW.** Specifically validates the Table Reservation logic using `IBASE_LOCK_WRITE` and `IBASE_PROTECTED`. Confirms that the C-loop correctly parses table names and options.
*   **`tests/savepoint_001.phpt`**: Validates the Savepoint lifecycle (Define -> Rollback -> define -> Release).
*   **`tests/savepoint_error_001.phpt`**: **NEW.** Verify robust error handling for invalid savepoint names and rolling back to non-existent savepoints.

## 3. Code Quality & Safety

*   **Memory Safety**: The TPB buffer handling was refactored to use a safe `TPB_MAX_SIZE` (2048) and check for overflow, preventing potential stack smashing when passing many table names.
*   **Input Validation**: Strict type checking on the `$options` array ensures invalid keys or values trigger appropriate PHP Warnings or TypeErrors.
*   **Backward Compatibility**: The new `fbird_trans_start` function exists alongside the legacy `ibase_trans` (which is now just a wrapper or alias where appropriate), ensuring no breaking changes for existing applications.

### Security Analysis

A security review of the new Transaction API identified and mitigated the following risks:

*   **Savepoint Injection**: The `fbird_savepoint($link, $name)` function maps to `SAVEPOINT <name>`. While Firebird DSQL generally handles identifiers safely, we strictly adhere to Firebird's identifier rules. PHP userland drivers (like Doctrine) are expected to quote/escape identifiers if they allow user input in savepoint names, but the underlying C implementation passes the string directly to `isc_dsql_execute_immediate`.
    *   *Mitigation*: We evaluated using parameter binding for DSQL savepoints, but Firebird syntax requires the name as a literal. We recommend higher-level abstractions avoid user-generated strings for savepoint names.
*   **Buffer Overflow in TPB**: The `tables` array logic iterates user input to build a binary TPB buffer.
    *   *Mitigation*: `TPB_MAX_SIZE` increased to 2048 and a strict bound check (`tpb_len < TPB_MAX_SIZE`) is enforced. If the user provides too many tables, the extension throws a Warning and aborts the transaction start, preventing stack corruption.
*   **Resource Confusion**: `fbird_trans_info` takes a resource handle.
    *   *Verification*: Standard Zend Resource validation ensures the handle is effectively a valid transaction pointer before access.

### Recommendations for Downstream Drivers

**Security Warning: Savepoint Names**
The `fbird_savepoint` family of functions performs direct DSQL execution. **Do not** pass unsanitized user input as a savepoint name.
*   **Bad:** `fbird_savepoint($trans, $userInput)`
*   **Good:** `fbird_savepoint($trans, "sp_" . bin2hex(random_bytes(8)))` OR `fbird_savepoint($trans, "FIXED_NAME")`

**Handling Type Errors**
`fbird_trans_start` generates standard PHP Warnings for invalid option types:
*   Invalid Key: `Warning: fbird_trans_start(): Unknown transaction option 'invalid_key'`
*   Invalid Value: `Warning: fbird_trans_start(): Invalid value for option 'lock_timeout'`
Check `error_get_last()` or handle warnings as exceptions if strict parsing is required.

## 4. Next Steps

With Phase 4 complete, the core modernization roadmap is effectively finished. The extension is now ready for:
1.  **Release 6.1.1**: Tagging and distribution.
2.  **Downstream Integration**: Updating Doctrine/Laravel drivers to use the new native Transaction and Savepoint APIs instead of raw DSQL hacks.
