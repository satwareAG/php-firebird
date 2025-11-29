# RFC: Comprehensive Transaction API Exposure

**Status:** Implemented
**Date:** 2025-11-27
**Implemented:** 2025-11-29
**Target:** php-firebird Extension (C++)
**Related:** Doctrine Firebird Driver "Optimized Transaction Handling"

## 1. Executive Summary

This RFC proposes expanding the `php-firebird` extension API to expose the **complete Firebird Transaction API** to PHP userland. This goes beyond basic commit/rollback to include Savepoints, explicit transaction parameters (TPB), table reservation locking, and detailed state inspection.

The goal is to provide a 1:1 mapping of capabilities between the Firebird engine and PHP, removing the need for raw DSQL hacks or "magic" default behaviors that limit advanced usage (e.g., high-concurrency locking strategies).

## 2. Problem Statement

### 2.1. Incomplete Transaction Control
While `ibase_trans()` allows basic isolation levels, it abstracts away the full power of the Firebird TPB (Transaction Parameter Block). Developers cannot easily:
-   Set explicit lock timeouts per transaction (WAIT vs NOWAIT with timeout).
-   Configure Table Reservation Locking (protecting specific tables).
-   Use modern Firebird 4.0+ features (Read Consistency).

### 2.2. The "SQL Savepoint" Fragility
Modern PHP frameworks (Doctrine, Laravel) rely on Nested Transactions via Savepoints. Current drivers use raw SQL (`SAVEPOINT X`), which the extension does not track. This causes **State Drift** where the PHP object thinks a transaction is nested, but the underlying handle might be in an inconsistent state or committed globally.

### 2.3. Opaque State
The `resource` returned by `ibase_trans()` is a black box. Drivers cannot ask "Is this transaction still active?" or "What is its ID?" without triggering an error by attempting a query.

## 3. Technical Recommendations

We propose exposing the complete API surface. `fbird_` aliases should be provided for all new and existing functions to modernise the namespace.

**Naming Convention Policy:**
- **Coexistence**: Existing `ibase_` functions retain their names for backward compatibility.
- **New Functions**: Use `fbird_` prefix (e.g., `fbird_savepoint`).
- **Aliases**: All existing `ibase_` functions will receive `fbird_` aliases to allow developers to migrate to a consistent namespace over time.
- **Deprecation**: No `ibase_` functions are deprecated in this RFC.

### 3.1. Expanded Transaction Start (TPB Exposure)

Enhance `ibase_trans` (or introduce `fbird_trans_start`) to accept a comprehensive configuration array, mapping directly to the TPB.

**Supported Options (New & Existing):**
Most options are optional. By default, `fbird_trans_start($db)` uses the standard default configuration. Advanced users can provide an array:

```php
$options = [
    'access_mode' => IBASE_READ | IBASE_WRITE,
    'isolation'   => IBASE_READ_COMMITTED | IBASE_REC_VERSION, // Existing
    'lock_resolution' => IBASE_WAIT, // Existing
    'lock_timeout' => 10, // NEW: Map to isc_tpb_lock_timeout
    'tables' => [ // NEW: Map to isc_tpb_lock_write / isc_tpb_lock_read
        'users' => IBASE_LOCK_WRITE,
        'orders' => IBASE_LOCK_READ
    ],
    'read_consistency' => true // NEW: Firebird 4.0+ isc_tpb_read_consistency
];
$trans = fbird_trans_start($db, $options);
```

### 3.2. Native Savepoint API (Wrappers)

Wrap DSQL savepoint execution to provide a type-safe, error-handled API.

**State Synchronization Note:**
These functions are **stateless wrappers** around DSQL. The PHP extension does *not* maintain an internal stack of savepoints. Applications (like Doctrine) must still track their nesting level logic, but should use these functions instead of raw SQL to ensuring proper handle validation and error mapping.

```c
// Starts a named savepoint. Returns true/false.
// Internally: isc_dsql_execute_immediate("SAVEPOINT <name>")
bool fbird_savepoint(resource $trans, string $name);

// Rolls back to named savepoint.
// Internally: isc_dsql_execute_immediate("ROLLBACK TO <name>")
bool fbird_rollback_savepoint(resource $trans, string $name);

// Releases a savepoint (makes it permanent in current stack).
// Internally: isc_dsql_execute_immediate("RELEASE SAVEPOINT <name>")
bool fbird_release_savepoint(resource $trans, string $name);
```

### 3.3. Complete Transaction Lifecycle

Ensure all lifecycle state transitions are exposed:

-   **Commit Retaining**: `fbird_commit_retaining($trans)` (Commit but keep context/cursors).
-   **Rollback Retaining**: `fbird_rollback_retaining($trans)` (Undo changes but keep context).
-   **Transaction Info**: `fbird_trans_info($trans)` mapping `isc_transaction_info`.

**Transaction Info Structure:**
```php
[
   'id' => (int) transaction_id,
   'state' => 'ACTIVE' | 'COMMITTED' | 'ROLLED_BACK',
   'isolation' => 'READ_COMMITTED',
   'lock_timeout' => 10,
   'is_snapshot' => false
]
```

## 4. Implementation Strategy

1.  **Constant Registration:** Register missing TPB constants (`IBASE_LOCK_TIMEOUT`, `IBASE_lock_write`, etc.) in `interbase.c`.
2.  **TPB Builder Upgrade:** Refactor `_php_ibase_def_trans` to support the new array-based options structure and constructing complex TPBs (specifically for Table Reservation).
3.  **Savepoint Wrappers:** Implement DSQL wrappers effectively.
4.  **Info Function:** Implement `fbird_trans_info` using `isc_transaction_info` C-API.
5.  **Testing & Q/A:**
    -   **Edge Cases**: Verify behavior with invalid savepoint names, double release, rollback to unknown savepoint.
    -   **Abuse Scenarios**: Test injection attempts in savepoint names (though DSQL should fail safely or be escaped).
    -   **Tests**:
        -   `tests/trans_tpb_001.phpt`: Lock timeout, table reservation, invalid options.
        -   `tests/savepoint_001.phpt`: Nesting, rollback, release, named savepoints.
        -   `tests/savepoint_error_001.phpt`: Duplicate names, invalid transitions.
        -   `tests/trans_info_001.phpt`: Verify state inspection throughout lifecycle.

## 5. Impact on Ecosystem

-   **Doctrine:** Can switch to `fbird_savepoint()` for nesting, removing raw SQL generation code.
-   **Laravel:** Firebird drivers for Laravel can implement robust `beginTransaction()` nesting.
-   **Legacy Apps:** No breaking changes (new functions).

## 6. References

-   [Firebird API Guide (Savepoints)](https://firebirdsql.org/file/documentation/reference_manuals/fblangref25-en/html/fblangref25-trans-savepoints.html)
-   `doctrine-firebird-driver` Issue #16 (Transaction Stability)
