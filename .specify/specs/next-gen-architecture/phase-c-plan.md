# Phase C Plan — Legacy Debt Cleanup + PDO Driver Skeleton

**Status**: Planning  
**Branch**: `feat/legacy-cleanup` (Part 1), `feat/pdo-fbird` (Part 2)  
**Depends on**: Phase A (merged), Phase B (merged)

---

## Part 1 — Legacy Debt Cleanup

### Goal
Remove all remaining `fb_safe_handle` fields and `isc_*()` function calls from the
extension, completing the OO API migration started in Phase A.

### Remaining Legacy Debt Inventory

#### `isc_*()` function calls (5 remaining)

| Call | File | Line | Replacement |
|------|------|------|-------------|
| `isc_sqlcode()` | `fbird_error.c` | 182 | Extract SQLCODE from `IStatus` via `getErrors()` array |
| `isc_array_lookup_bounds()` | `fbird_query_array.c` | 88 | Query `RDB$FIELD_DIMENSIONS` system table via OO API |
| `isc_encode_timestamp()` | `fbird_query_array.c` | 391 | Inline `ISC_TIMESTAMP` struct assignment from `struct tm` |
| `isc_encode_sql_date()` | `fbird_query_array.c` | 441 | Inline `ISC_DATE` assignment using `IUtil::encodeDate()` |
| `isc_encode_sql_time()` | `fbird_query_array.c` | 459 | Inline `ISC_TIME` assignment using `IUtil::encodeTime()` |

Note: `isc_event_block()`, `isc_wait_for_event()`, `isc_event_counts()` in
`firebird_utils.cpp` are already wrapped behind `fbe_*()` C bridges — these are
internal implementation details of the wrappers and acceptable to keep since the
OO API has no event equivalents.

#### `fb_safe_handle` struct fields (4 remaining)

| Struct | Field | File | Status |
|--------|-------|------|--------|
| `fbird_db_link` | `handle` | `php_fbird_includes.h:122` | Write-only after Phase 1; used in `fbird_connection.c`, `fbird_events.c`, `fbird_transaction.c` |
| `fbird_transaction` | `handle` | `php_fbird_includes.h:140` | Used in `fbird_transaction.c`, `fbird_query_exec.c`, `fbird_connection.c` |
| `fbird_blob` | `bl_handle` | `php_fbird_includes.h:156` | 2 remaining assignments in `fbird_result.c`, `fbird_query_bind.c` |
| `_ib_query` | `stmt` | `php_fbird_includes.h:224` | Active `isc_stmt_handle` for prepared statements |

#### `fb_safe_handle` union itself

| Location | Status |
|----------|--------|
| `php_fbird_includes.h:115-119` | Remove once all 4 struct fields above are eliminated |

### Approach

1. **T1**: Replace `isc_sqlcode()` with `IStatus`-based SQLCODE extraction via new `fbc_sqlcode()` C bridge
2. **T2**: Replace `isc_encode_*()` with `IUtil::encodeDate/Time/encodeTimeStamp()` via new C bridges
3. **T3**: Replace `isc_array_lookup_bounds()` with OO system table query
4. **T4**: Remove `bl_handle` from `fbird_blob`, replace remaining checks with `fbb_blob` null checks
5. **T5**: Remove `handle` from `fbird_db_link` and `fbird_transaction`, replace all usages with OO helpers
6. **T6**: Remove dead `_ib_query.stmt` field, migrate to `fbs_statement` OO wrapper
7. **T7**: Remove `fb_safe_handle` union entirely if no remaining users

---

## Part 2 — PDO Driver Skeleton

### Goal
Create a thin PDO driver (`pdo_fbird`) that integrates the existing `fbird_*` C
extension into PHP's PDO framework, using the `fbird:` DSN prefix to avoid
collision with the bundled `pdo_firebird` driver.

### Architecture

```
┌─────────────────────────────────────────┐
│  PHP userland                           │
│  $pdo = new PDO('fbird:host=...', ...) │
├─────────────────────────────────────────┤
│  pdo_fbird.so  (Layer 3)               │
│  - pdo_fbird_driver.c   (connection)   │
│  - pdo_fbird_stmt.c     (statements)   │
│  - pdo_fbird_error.c    (SQLSTATE map) │
│  - config.m4 / config.w32              │
├─────────────────────────────────────────┤
│  firebird.so  (Layer 1 + Layer 2)      │
│  - fbird_* C functions                  │
│  - Firebird\* OOP classes               │
│  - firebird_utils.cpp OO wrappers       │
└─────────────────────────────────────────┘
```

### Key Design Decisions

- **Separate `.so`**: `pdo_fbird.so` is a separate shared module that depends on `firebird.so`
- **DSN prefix**: `fbird:` (not `firebird:`) to avoid collision with bundled `pdo_firebird`
- **Reuse OO wrappers**: PDO driver calls `fbc_*`, `fbt_*`, `fbs_*`, `fbb_*` C bridges directly
- **Custom attributes**: `PDO::FBIRD_ATTR_*` constants for Firebird-specific features (dialect, charset, role, page buffers)
- **SQLSTATE mapping**: Map Firebird GDS error codes to standard SQLSTATE codes

### Tasks (T8–T18)

- **T8**: Create `pdo_fbird/` directory structure with `config.m4`, `config.w32`, `php_pdo_fbird.h`
- **T9**: Implement `pdo_fbird.c` — module init, `fbird:` DSN registration
- **T10**: Implement `pdo_fbird_driver.c` — connection factory (parse DSN, call `fbc_connect()`)
- **T11**: Implement transaction support (`beginTransaction`, `commit`, `rollback`)
- **T12**: Implement `pdo_fbird_stmt.c` — `prepare()`, `execute()`, parameter binding
- **T13**: Implement fetch methods (`fetch`, `fetchColumn`, `fetchAll`)
- **T14**: Implement `pdo_fbird_error.c` — SQLSTATE error mapping from GDS codes
- **T15**: Implement `PDO::FBIRD_ATTR_*` custom attribute constants
- **T16**: Implement LOB/BLOB support via PDO streams
- **T17**: Add `.phpt` tests for all PDO operations
- **T18**: Update stubs, documentation, and CI matrix

---

## Validation Gates

### After Part 1
- Full 12-target test matrix green
- ASan 3/3 PASS
- Valgrind 0 definitely/indirectly lost
- `grep -rn 'fb_safe_handle' --include="*.h"` returns 0 results (or only in `_ib_query.stmt` if T6 deferred)

### After Part 2
- Full 12-target test matrix green (existing + new PDO tests)
- PDO standard operations work: connect, query, prepare, execute, fetch, transactions
- ASan + Valgrind clean
- Windows build (config.w32) compiles
