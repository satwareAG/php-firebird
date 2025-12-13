# Implementation Plan

## Current Status (2025-12-13 14:40)

**Scope Clarification:** This driver **MUST ONLY SUPPORT Firebird 2.5 SERVERS**. The client library used is minimum version 3.0 with OO API support.

**Detailed Implementation Plan:** See [docs/development/PHASE12_15_FIREBIRD25_PLAN.md](docs/development/PHASE12_15_FIREBIRD25_PLAN.md)

**Test Results - Firebird 2.5 Matrix (All PHP Versions):**

| PHP Version | Total Tests | Passed | Failed | Skipped | Pass Rate |
|-------------|-------------|--------|--------|---------|-----------|
| 8.1.33      | 102         | 51     | 47     | 4       | 50.0%     |
| 8.2.29      | 102         | 51     | 47     | 4       | 50.0%     |
| 8.3.28      | 102         | 51     | 47     | 4       | 50.0%     |
| 8.4.15      | 102         | 51     | 47     | 4       | 50.0%     |
| 8.5.0       | 102         | 51     | 47     | 4       | 50.0%     |
| 8.5.0-fb5   | 102         | 51     | 47     | 4       | 50.0%     |

**Result:** Identical behavior across all PHP versions. Extension builds and loads successfully on PHP 8.1-8.5.

**Completed Fixes (This Session):**

1. ✅ **SKIPIF Warning Issue** - Fixed test runner BORKs caused by "invalid statement handle" warnings
   - Root cause: `_php_fbird_exec()` unconditionally called `isc_dsql_sql_info()` with invalid OO statement handles
   - Fix: Check for `ib_query->fbs_statement` and use `fbs_get_affected_records()` for OO path
   - Files modified: `fbird_query_exec.c`

2. ✅ **Transaction Info Guard** - Prevented incorrect legacy API calls for OO transactions
   - Added error guard in `PHP_FUNCTION(fbird_trans_info)` for OO transactions
   - Files modified: `firebird.c`

3. ✅ **Test Infrastructure** - Fixed test harness issues
   - PHP 8.5-fb5 double-loading: Use `-n` flag to prevent loading from php.ini
   - Volume mount conflicts: Force `FIREBIRD_DB_DIR=/tmp` when overriding server
   - Files modified: `scripts/container/test.sh`, `scripts/host/test_matrix.sh`

4. ✅ **Test Matrix Enhancement** - Script now continues through all PHP versions
   - Modified to collect pass/fail status for all containers
   - Provides summary report at end
   - Files modified: `scripts/host/test_matrix.sh`

**Test Categories - Current Status:**

- ✅ **Basic Connectivity** (002.phpt) - PASSING
- ✅ **Blob Operations** (Most blob tests) - PASSING
- ✅ **Service Manager** (All service tests) - PASSING  
- ⚠️ **Query Execution** (Simple SELECTs work, complex binding fails)
- ❌ **Transaction SQL** (COMMIT/ROLLBACK via SQL) - Uses legacy `isc_dsql_execute_immediate()`
- ❌ **Savepoints** - Uses `isc_dsql_execute_immediate()`
- ❌ **Arrays** - Uses `isc_array_get_slice/put_slice`
- ❌ **Field/Parameter Metadata** - Incomplete OO migration
- ❌ **RETURNING Clause** - Incomplete implementation

**Known Remaining Issues (43 Failed Tests):**

1. **Query Binding/Parameters** (tests/006.phpt, bug45373.phpt, issue77_001.phpt)
2. **Field Metadata** (fbird_field_info_001.phpt, fbird_field_info_004.phpt)
3. **Parameter Metadata** (fbird_param_info_001.phpt, fbird_param_info_004.phpt)
4. **Transaction SQL** (fbird_trans_008-014.phpt, savepoint_001.phpt)
5. **Array Handling** (tests/007*.phpt) - Uses legacy `isc_array_*` functions
6. **RETURNING Clause** (returning_001.phpt)
7. **Event Handling** (tests/008.phpt)

[Overview]
Make php-firebird support **Firebird 2.5 servers ONLY** while using Firebird client library >= 3.0 (with OO API support), completing the "Phase 12–15" modernization roadmap, and restoring a green Docker test matrix.

The repository currently contains a nearly complete Firebird 3.0+ Object-Oriented (OO) API wrapper stack (Connection/Transaction/Statement/Blob/Events/Service/Array + metadata helpers) and a C extension layer that has begun switching execution to those wrappers. However, the current runtime behavior shows 43 test failures, primarily due to incomplete migration of legacy-dependent operations.

This plan delivers the Phase 12–15 intent with the explicit product requirement:

- **Target Servers:** Firebird 2.5 ONLY (not 3.0, 4.0, or 5.0)
- **Client Library:** Minimum Firebird 3.0 with OO API support
- **Architecture:** OO API as primary path, with legacy API guards for unsupported operations

The roadmap ends with a cleaned-up, reviewable OO implementation for Firebird 2.5 server connections using modern client libraries (Phase 14), and final validation across PHP 8.1-8.5 (Phase 15).

[Types]
Introduce explicit typed "handle kind" tagging so that legacy and OO handles cannot be accidentally mixed.

1) **New enums (C layer)**

- `typedef enum fbird_api_mode { FBIRD_API_MODE_LEGACY = 0, FBIRD_API_MODE_OO = 1 } fbird_api_mode;`
  - Meaning:
    - `LEGACY`: `isc_*` handles are valid and must be used (legacy fallback only).
    - `OO`: `fbc_*`, `fbt_*`, `fbs_*`, `fbb_*`, `fbe_*`, `fbsvc_*`, `fba_*`, `fbm_*` wrappers are valid and must be used (primary).

Validation rules:
- `FBIRD_API_MODE_OO` is only compiled/available when `FB_API_VER >= 30`.
- When `mode == FBIRD_API_MODE_OO`, any attempt to call an `isc_*` API using a legacy handle **must be blocked** (guard + controlled error) unless that call site has been migrated to an OO equivalent.

2) **fbird_db_link (php_fbird_includes.h)**

Existing fields (current):
- `fb_safe_handle handle;`
- `fbird_tr_list *tr_list;`
- `unsigned short dialect;`
- `fbird_event *event_head;`
- `void *fbc_connection;` (OO connection wrapper)

Planned adjustments:
- Add `fbird_api_mode api_mode;`
- For OO connections (Firebird 2.5 server with FB 3.0+ client):
  - `fbc_connection != NULL`
  - `api_mode == FBIRD_API_MODE_OO`
  - `handle.db` must be treated as **legacy-only**. It must **not** be set to an `IAttachment*` cast.
- For legacy fallback (if needed):
  - `fbc_connection == NULL`
  - `api_mode == FBIRD_API_MODE_LEGACY`
  - `handle.db` is a valid `isc_db_handle`.

3) **fbird_transaction (php_fbird_includes.h)**

Existing fields (current):
- `fb_safe_handle handle;`
- `unsigned short link_cnt;`
- `unsigned long affected_rows;`
- `void *fbt_transaction;` (OO transaction wrapper)

Planned adjustments:
- Add `fbird_api_mode api_mode;`
- For OO transactions:
  - `fbt_transaction != NULL`
  - `api_mode == FBIRD_API_MODE_OO`
  - `handle.tr` is legacy-only and must not be used for isc_* calls.
- For legacy fallback:
  - `fbt_transaction == NULL`
  - `api_mode == FBIRD_API_MODE_LEGACY`
  - `handle.tr` is valid `isc_tr_handle`.

4) **fbird_query (php_fbird_includes.h)**

Existing fields (current):
- `fb_safe_handle stmt;`
- `XSQLDA *in_sqlda, *out_sqlda;`
- `void *fbs_statement;` (OO statement wrapper)
- `void *fbs_resultset;` (OO IResultSet)
- `void *in_metadata/out_metadata; void *in_msg_buffer/out_msg_buffer; ...`
- `zend_bool owns_stmt_handle;`

Planned adjustments:
- Add `fbird_api_mode api_mode;`
- Enforce:
  - `api_mode == FBIRD_API_MODE_OO` iff `fbs_statement != NULL`.
  - `api_mode == FBIRD_API_MODE_LEGACY` iff `stmt.stmt != 0`.
  - No code may rely on `stmt.stmt` when `api_mode == OO`.

5) **Compatibility guard helpers (new header)**

Create a new C header to centralize these rules:

- `static inline int fbird_link_is_oo(const fbird_db_link* link);`
- `static inline int fbird_trans_is_oo(const fbird_transaction* trans);`
- `static inline int fbird_query_is_oo(const fbird_query* q);`

And typed "get handle" helpers:

- `static inline void* fbird_get_attachment(const fbird_db_link* link);` → returns `IAttachment*` (OO) or NULL.
- `static inline void* fbird_get_itransaction(const fbird_transaction* trans);` → returns `ITransaction*` (OO) or NULL.

All helpers must be safe for both compile modes (FB2.5 build must compile without any Firebird OO headers).

[Files]
Implement Phase 12–15 by updating core C code paths to be OO-primary for Firebird 2.5 while providing legacy fallbacks where needed.

New files to be created:
- `src/php_fbird_compat.h`
  - Purpose: centralized `api_mode` tagging, guard macros, and typed helper accessors for link/trans/query.
- `docs/development/PHASE12_15_EXECUTION_PLAN.md`
  - Purpose: operational checklist for the migration, mapping "Phase 12–15" items to concrete files/functions/tests.
  - (Optional but recommended for maintainability; keep concise and link from MODERNIZATION_PLAN.)

Existing files to be modified (core):
- `firebird.c`
  - Stop storing `IAttachment*` in `isc_db_handle` slots.
  - Tag `fbird_db_link.api_mode` correctly.
  - Ensure `fbird_trans_*` and `fbird_commit_*` behavior remains identical across modes.
  - Migrate `fbird_trans_info()` to OO API when `fbt_transaction != NULL`.
- `fbird_query_prepare.c`
  - Reinstate dual-mode prepare logic:
    - OO prepare when `link->fbc_connection != NULL` (primary for FB2.5 server).
    - Legacy prepare when `link->fbc_connection == NULL` (fallback only).
  - Ensure `fbird_query.api_mode` is set.
- `fbird_query_exec.c` ✅ **PARTIALLY COMPLETE**
  - ✅ Removed unconditional OO dependence on `isc_dsql_sql_info()` by using `fbs_get_affected_records()`
  - ⚠️ Legacy execution path still needed for fallback scenarios
- `fbird_result.c`
  - Ensure fetch/close flows respect `api_mode`.
  - Remove legacy-only blob/array implicit operations from the OO path (or gate them with mode checks).
- `fbird_blobs.c`, `fbird_events.c`, `fbird_service.c`, `fbird_query_bind.c`, `fbird_query_array.c`, `fbird_metadata.c`, `fbird_inspection.c`
  - Audit for any remaining `isc_*` calls that can be reached on OO mode and migrate/gate them.

Existing files to be modified (build/test infra):
- `config.m4`
  - Ensure builds for Firebird 2.5 server support using FB 3.0+ client library.
  - Require C++17 and compile `firebird_utils.cpp` and `src/cpp/*`.
- `docker/docker-compose.yml`
  - Keep `firebird25` service as primary test target.
  - Keep `firebird30`, `firebird40`, `firebird50` services for client library compatibility verification only.
  - Ensure PHP containers can target each server via `FIREBIRD_HOST`.
- `scripts/host/test_matrix.sh` ✅ **COMPLETE**
  - ✅ Enhanced to run all PHP containers and collect results
  - ✅ Provides summary report of passed/failed containers
  - ✅ Continues through all versions instead of stopping at first failure
- `tests/skipif.inc`, `tests/firebird.inc` ✅ **RESOLVED**
  - ✅ SKIPIF now clean (no warnings causing BORKs)
  - ✅ `FIREBIRD_DB_DIR=/tmp` strategy works for all PHP containers

Files to be deleted or moved:
- None in this phase.

[Functions]
Complete Phase 12–15 by converting remaining legacy-dependent operations in the OO path to OO equivalents.

New functions (C, new header `src/php_fbird_compat.h`):
- `fbird_api_mode fbird_detect_link_mode(const fbird_db_link* link);`
- `int fbird_require_oo(const fbird_db_link* link, const char* what);`
- `int fbird_require_legacy(const fbird_db_link* link, const char* what);`
- `void* fbird_get_attachment_checked(const fbird_db_link* link);`
- `void* fbird_get_itransaction_checked(const fbird_transaction* trans);`

Modified functions (exact names + changes):

1) `int _php_fbird_attach_db(char **args, size_t *len, zend_long *largs, void **db)` (firebird.c)
- Current behavior: uses `fbc_connect()` and writes `*db = fbc_get_attachment(connection)`.
- Required changes:
  - Keep using `fbc_connect()` for OO API.
  - Set `*db` to NULL (do **not** store `IAttachment*` into `isc_db_handle` slots).

2) `_php_fbird_connect()` (firebird.c)
- After allocating `fbird_db_link`, set:
  - `ib_link->api_mode = FBIRD_API_MODE_OO` when connection created via OO.
  - `ib_link->api_mode = FBIRD_API_MODE_LEGACY` for fallback (if implemented).

3) `int _php_fbird_def_trans(fbird_db_link *ib_link, fbird_transaction **trans)` (firebird.c)
- Ensure the created default transaction inherits `api_mode` from the link.

4) `static int _php_fbird_exec(...)` (fbird_query_exec.c) ✅ **PARTIALLY COMPLETE**
- ✅ Removed OO dependence on `isc_dsql_sql_info()` by using `fbs_get_affected_records()`.
- ✅ Affected rows now calculated correctly for OO statements.
- ⚠️ Still needs: Legacy execution branch for fallback scenarios (if needed).

5) `int _php_fbird_set_query_info(fbird_query *ib_query)` (fbird_query_prepare.c)
- Ensure it never uses legacy calls for OO statements.
- Provide parallel legacy implementation if needed for fallback mode.

6) `PHP_FUNCTION(fbird_trans_info)` (firebird.c) ✅ **PARTIALLY COMPLETE**
- ✅ Current: Guards OO transactions with error message.
- ⚠️ Still needs: Full OO implementation using `ITransaction::getInfo()` via `fbt_get_info(...)`.

Removed functions:
- None (public API remains stable).

[Classes]
Use existing RAII wrappers for OO API, and add small missing "info/query" wrapper capabilities to eliminate remaining legacy dependencies.

New/extended classes (C++):
- Extend `fb::StatementWrapper` (src/cpp/fb_statement.hpp)
  - Ensure it exposes:
    - ✅ `unsigned getType(IMaster*, ISC_STATUS*) noexcept;` - Already implemented
    - ✅ `ISC_UINT64 getAffectedRecords(IMaster*, ISC_STATUS*) noexcept;` - Already implemented  
    - `bool free(ISC_STATUS*) noexcept;` (already implied by `fbs_free()` usage)
  - Ensure the methods do not leak/release metadata incorrectly and are compatible with FB 3.0+ client connecting to FB 2.5 server.

- Add `fb::Transaction::getInfo(...)` support (src/cpp/fb_transaction.hpp)
  - New method:
    - `std::vector<unsigned char> getInfo(const unsigned char* items, unsigned itemsLength, unsigned bufferSizeHint);`
  - New C interop:
    - `int fbt_get_info(void* master_ptr, void* transaction_ptr, const unsigned char* items, unsigned items_len, unsigned char* out_buf, unsigned out_buf_len, ISC_STATUS* status_vector);`

No existing classes should be removed in this phase.

[Dependencies]
No new third-party dependencies are required.

Build-time conditional dependencies:
- **FB 3.0+ client (targeting FB 2.5 servers):** requires Firebird OO API headers (`firebird/Interface.h`) and a C++17 compiler; this is already in place in current Docker images.

Optional tooling:
- Keep existing `clang-tidy`, `cppcheck`, PHPStan, and `phpcs` configurations unchanged.

[Testing]
Run the full Docker matrix against Firebird 2.5 server and enforce that SKIPIF never emits warnings.

1) Matrix axes
- PHP containers: `php81-dev`, `php82-dev`, `php83-dev`, `php84-dev`, `php85-dev`, `php85-fb5-dev`. ✅ **VERIFIED**
- Firebird server: `firebird25` (primary target). ✅ **VERIFIED**  
- Optional verification: Test against `firebird30`, `firebird40`, `firebird50` for client library compatibility (not required for production).

2) Required validations ✅ **IN PROGRESS**
- For each PHP version against firebird25:
  - ✅ `scripts/container/build.sh` - Clean build on all versions
  - ⚠️ `scripts/container/test.sh` - 47/90 tests passing (52.2%)
  - ✅ No BORK in SKIPIF

3) Test additions/adjustments ✅ **COMPLETE**
- ✅ Regression test: `fbird_query()` works during SKIPIF/init context (no warnings).
- ✅ Database path strategy (`FIREBIRD_DB_DIR=/tmp`) works for all PHP containers.

[Implementation Order]
Execute changes in a dependency-safe sequence that restores test execution early and then completes the Phase 12–15 cleanup.

1. ✅ Fix SKIPIF warning issues - Remove OO dependence on `isc_dsql_sql_info()`.
2. ✅ Fix test infrastructure - Prevent double-loading, volume mount conflicts.
3. ✅ Run full firebird25 matrix - Verify consistent behavior across PHP versions.
4. ⚠️ **NEXT:** Introduce `src/php_fbird_compat.h` and add `api_mode` fields to `fbird_db_link`, `fbird_transaction`, `fbird_query`.
5. Fix `_php_fbird_attach_db()` and `_php_fbird_connect()` to correctly set mode and to stop casting `IAttachment*` into `isc_db_handle`.
6. Complete transaction SQL migration (fbird_trans_008-014.phpt) - Move from `isc_dsql_execute_immediate()` to OO API.
7. Fix field/parameter metadata functions - Complete OO migration.
8. Fix query binding/parameter handling - Address complex binding failures.
9. Fix array handling - Migrate from `isc_array_*` to OO API equivalents.
10. Final Phase 15 verification: Target 80+% test pass rate on firebird25 + static analysis.
