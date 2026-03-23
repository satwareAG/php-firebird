# Deprecation Audit — php-firebird Extension

**Date:** 2026-03-23
**Scope:** All deprecated/legacy patterns in the satwareAG/php-firebird driver

---

## 1. Legacy ISC Handle Fields

### 1a. `isc_db_handle` in `fbird_db_link`

- **Location:** `php_fbird_includes.h` — `fbird_db_link` struct
- **Field:** `isc_db_handle db_handle` (legacy C API database handle)
- **Status:** Still present in struct alongside OO API `fbc_connection` pointer
- **Impact:** Unused in OO API path; only referenced by legacy event/transaction code
- **Action:** Remove once events and multi-db transactions are migrated to OO API

### 1b. `legacy_handle_` in `fb::Connection`

- **Location:** `src/cpp/fb_connection.hpp:302`
- **Field:** `isc_db_handle legacy_handle_ = 0`
- **Status:** Stored but only accessed via `getLegacyHandlePtr()` for events and `ISC_TEB`-based transactions
- **Impact:** Bridge between OO API Connection wrapper and legacy C API callers
- **Action:** Remove after `fbird_events.c` and `fbird_transaction.c` multi-db path are modernized

### 1c. `isc_tr_handle` / `isc_stmt_handle` in structs

- **Location:** `php_fbird_includes.h` — `fbird_transaction`, `fbird_query` structs
- **Fields:** `isc_tr_handle tr_handle`, `isc_stmt_handle stmt` (legacy handles)
- **Status:** Present alongside OO API pointers (`fbt_transaction`, `fbs_statement`)
- **Impact:** Legacy handles are zero/unused when OO API is active
- **Action:** Remove after all callers use OO API exclusively

---

## 2. `fbc_get_legacy_handle_ptr()` Bridge

- **Location:** `firebird_utils.cpp:703`, declared in `firebird_utils.h`
- **Purpose:** Extracts `&legacy_handle_` from `fb::Connection` for legacy C API calls
- **Callers:**
  - `fbird_events.c:201,213,406,469` — `fbe_wait_for_event()` needs `isc_db_handle*`
  - `fbird_transaction.c:710` — `ISC_TEB.db_ptr` for `isc_start_multiple()`
- **Impact:** Only bridge keeping legacy handle alive; removing it requires OO API alternatives
- **Action:** Remove after events use `IEvents` OO API and multi-db transactions use `fbt_start()`

---

## 3. `ISC_TEB` Struct for Multi-Database Transactions

- **Location:** `php_fbird_includes.h` (typedef), `fbird_transaction.c:682,690,710`
- **Purpose:** `isc_start_multiple()` requires an array of `ISC_TEB` structs with `isc_db_handle*`
- **Pattern:** Allocates `ISC_TEB` array, fills with legacy db handles and TPB, calls `isc_start_multiple()`
- **Impact:** Last real dependency on legacy Firebird C API handles for transaction start
- **Action:** Replace with per-connection `fbt_start()` OO API calls; multi-db transactions are rare and can be handled by starting separate transactions on each connection

---

## 4. `fba_array_lookup_bounds()` — Raw `isc_array_*` Calls

- **Location:** `fbird_query_array.c`
- **Purpose:** Wraps `isc_array_lookup_bounds()` and related `isc_array_*` functions
- **Status:** Firebird OO API does **not** provide array field equivalents — these are Firebird-API-level legacy with no replacement
- **Impact:** Required for array field support; cannot be modernized until Firebird provides OO API array interfaces
- **Action:** Document as Firebird-API-level legacy; keep as-is until upstream provides alternatives

---

## 5. `FBIRD_API_MODE_LEGACY` Compat Layer

- **Location:** `src/php_fbird_compat.h:25-28`
- **Content:** `typedef enum fbird_api_mode { FBIRD_API_MODE_LEGACY = 0, FBIRD_API_MODE_OO = 1 }`
- **Also:** `fbird_get_link_mode()`, `fbird_get_trans_mode()`, `fbird_get_query_mode()` helpers
- **Also:** `FBIRD_REQUIRE_LEGACY_*` guard macros (lines ~171-199)
- **Status:** Never actually used — all connections are OO API; the enum and guards are dead code
- **Impact:** Adds confusion; suggests legacy mode is supported when it is not
- **Action:** Remove `FBIRD_API_MODE_LEGACY` enum value, remove `FBIRD_REQUIRE_LEGACY_*` macros, simplify mode detection to always return OO

---

## 6. `get_statement_interface` Global

- **Location:** `php_fbird_includes.h:101` (field in globals), `firebird.c:756` (initialization)
- **Purpose:** Stores `fb_get_statement_interface` symbol from fbclient for legacy statement creation
- **Status:** Loaded at MINIT but never used — all statements use OO API `fbs_prepare()`
- **Referenced:** `fbird_metadata.c:104` (comment noting it's disabled)
- **Impact:** Dead code; wastes a global slot
- **Action:** Remove field from globals and the `dlsym()` call

---

## 7. `void*` Opaque Pointer Pattern in `firebird_utils.h` C API

- **Location:** `firebird_utils.h` — all `fbc_*`, `fbt_*`, `fbs_*`, `fbb_*` functions
- **Pattern:** All parameters use `void*` for connection/transaction/statement/blob pointers
- **Example:** `int fbc_disconnect(void* connection, ISC_STATUS* status_vector)`
- **Impact:** No compile-time type safety; easy to pass wrong pointer type silently
- **Action:** Replace `void*` with forward-declared opaque struct typedefs:
  ```c
  typedef struct fbc_connection_t fbc_connection_t;
  typedef struct fbt_transaction_t fbt_transaction_t;
  typedef struct fbs_statement_t fbs_statement_t;
  typedef struct fbb_blob_t fbb_blob_t;
  ```
  Update all function signatures and call sites in `fbird_*.c` and `pdo_fbird/*.c`

---

## 8. Shutdown Crash — Dangling `master_` Pointer

- **Location:** `src/cpp/fb_connection.hpp:301`, `firebird_utils.cpp:285`
- **Root Cause:** `fb::Connection` caches `master_` as a raw pointer to `IBG(master_instance)`. During MSHUTDOWN, the Firebird client library may be unloaded, making `master_` dangling. PDO objects destroyed after MSHUTDOWN call `detachNoThrow()` which dereferences the dangling pointer.
- **Reproduction:** Any PDO connection left open when `fbird_drop_db()` is called on a different resource handle, or when PHP shuts down with open PDO connections after `include` of test helpers that create fbird connections.
- **Fix:** Make `getMaster()` in `firebird_utils.cpp` check `IBG(in_mshutdown)` and return `nullptr`. The existing `if (master_)` guard in `detachNoThrow()` then safely skips the detach. The Firebird client library cleans up on process exit.
- **Status:** **FIXED** (2026-03-23)

---

## Priority Summary

| # | Pattern | Priority | Complexity | Blocked By |
|---|---------|----------|------------|------------|
| 8 | Shutdown crash (dangling master_) | **P0** | S | — |
| 5 | FBIRD_API_MODE_LEGACY dead code | P1 | S | — |
| 6 | get_statement_interface dead code | P1 | S | — |
| 7 | void* opaque pointers | P1 | M | — |
| 3 | ISC_TEB multi-db transactions | P1 | L | — |
| 2 | fbc_get_legacy_handle_ptr bridge | P1 | M | #3, #9 (events) |
| 1 | Legacy ISC handle fields | P1 | M | #2, #3 |
| 9 | fbird_events.c legacy API | P1 | L | — |
| 4 | isc_array_* calls | P2 | — | Firebird upstream |
