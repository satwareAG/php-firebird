# Implementation Plan

[Overview]
Make php-firebird support Firebird servers 2.5 through 5.0 while completing the “Phase 12–15” modernization roadmap (OO API primary for FB 3.0+ builds, legacy preserved for FB 2.5 builds), and restore a green Docker test matrix.

The repository currently contains a nearly complete Firebird 3.0+ Object-Oriented (OO) API wrapper stack (Connection/Transaction/Statement/Blob/Events/Service/Array + metadata helpers) and a C extension layer that has begun switching execution to those wrappers. However, the current runtime behavior shows a hard break: most tests BORK at SKIPIF time, due to the test harness calling `fbird_query()` during `tests/firebird.inc:init_db()` and encountering “invalid statement handle”. Root cause is a partial “legacy removal” where `fbird_query_exec.c` unconditionally errors out for legacy connections and still contains a few legacy-only operations (e.g. `isc_dsql_sql_info`) that will be invalid under the OO-only handle scheme.

This plan delivers the Phase 12–15 intent, but with the explicit product requirement that the extension must support Firebird 2.5 through 5.0:

- **For builds against Firebird 3.0+ client headers (FB_API_VER >= 30):** OO API becomes the primary path (Phase 12) and the remaining legacy call sites are migrated (Phase 13) so that the extension does not rely on legacy statement/transaction handles for correctness.
- **For builds against Firebird 2.5 client headers (FB_API_VER < 30):** the legacy isc_* API remains the primary and only implementation. Any “legacy removal” is strictly conditional, so 2.5 support is not lost.

The roadmap ends with a cleaned-up, reviewable separation between FB2.5 legacy implementation and FB3+ OO implementation (Phase 14), and a final matrix validation across servers/clients/PHP versions (Phase 15).

[Types]
Introduce explicit typed “handle kind” tagging so that FB2.5 legacy handles and FB3+ OO handles cannot be accidentally mixed.

1) **New enums (C layer)**

- `typedef enum fbird_api_mode { FBIRD_API_MODE_LEGACY = 0, FBIRD_API_MODE_OO = 1 } fbird_api_mode;`
  - Meaning:
    - `LEGACY`: `isc_*` handles are valid and must be used.
    - `OO`: `fbc_*`, `fbt_*`, `fbs_*`, `fbb_*`, `fbe_*`, `fbsvc_*`, `fba_*`, `fbm_*` wrappers are valid and must be used.

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
- For FB3+ OO connections:
  - `fbc_connection != NULL`
  - `api_mode == FBIRD_API_MODE_OO`
  - `handle.db` must be treated as **legacy-only**. It must **not** be set to an `IAttachment*` cast.
- For legacy (FB2.5) connections:
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
- For legacy transactions:
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

And typed “get handle” helpers:

- `static inline void* fbird_get_attachment(const fbird_db_link* link);` → returns `IAttachment*` (OO) or NULL.
- `static inline void* fbird_get_itransaction(const fbird_transaction* trans);` → returns `ITransaction*` (OO) or NULL.

All helpers must be safe for both compile modes (FB2.5 build must compile without any Firebird OO headers).

[Files]
Implement Phase 12–15 by updating core C code paths to be OO-primary under FB3+ while keeping legacy implementations for FB2.5.

New files to be created:
- `src/php_fbird_compat.h`
  - Purpose: centralized `api_mode` tagging, guard macros, and typed helper accessors for link/trans/query.
- `docs/development/PHASE12_15_EXECUTION_PLAN.md`
  - Purpose: operational checklist for the migration, mapping “Phase 12–15” items to concrete files/functions/tests.
  - (Optional but recommended for maintainability; keep concise and link from MODERNIZATION_PLAN.)

Existing files to be modified (core):
- `firebird.c`
  - Stop storing `IAttachment*` in `isc_db_handle` slots.
  - Tag `fbird_db_link.api_mode` correctly.
  - Ensure `fbird_trans_*` and `fbird_commit_*` behavior remains identical across modes.
  - Migrate `fbird_trans_info()` to OO API when `fbt_transaction != NULL`.
- `fbird_query_prepare.c`
  - Reinstate dual-mode prepare logic:
    - OO prepare when `link->fbc_connection != NULL` (FB3+ builds).
    - Legacy prepare when `link->fbc_connection == NULL` (including FB2.5 builds).
  - Ensure `fbird_query.api_mode` is set.
- `fbird_query_exec.c`
  - Remove the unconditional “legacy not supported” error.
  - Implement dual-mode execution:
    - OO execute path when `fbs_statement && trans->fbt_transaction`.
    - Legacy execute path otherwise.
  - Migrate **remaining legacy-only post-exec operations** to OO equivalents (notably affected rows / statement type), so OO statements never require `isc_dsql_sql_info()`.
- `fbird_result.c`
  - Ensure fetch/close flows respect `api_mode`.
  - Remove legacy-only blob/array implicit operations from the OO path (or gate them with mode checks).
- `fbird_blobs.c`, `fbird_events.c`, `fbird_service.c`, `fbird_query_bind.c`, `fbird_query_array.c`, `fbird_metadata.c`, `fbird_inspection.c`
  - Audit for any remaining `isc_*` calls that can be reached on OO mode and migrate/gate them.

Existing files to be modified (build/test infra):
- `config.m4`
  - Detect FB 2.5 vs 3+ client headers.
  - For FB2.5 builds: do not require C++17 / OO headers.
  - For FB3+ builds: enable C++17 and compile `firebird_utils.cpp` and `src/cpp/*`.
- `docker/docker-compose.yml`
  - Keep `firebird25`, `firebird30`, `firebird40`, `firebird50` services.
  - Ensure PHP containers can target each server via `FIREBIRD_HOST`.
- `scripts/host/test_matrix.sh`
  - Add an execution mode to run each PHP container against each server (at minimum: 25/30/40/50).
  - Provide a “fast” mode for default server only.
- `tests/skipif.inc`, `tests/firebird.inc`
  - Ensure SKIPIF and init_db behave deterministically across server versions.
  - Ensure `FIREBIRD_DB_DIR`/volume strategy works for all PHP containers.

Files to be deleted or moved:
- None in this phase; Phase 14 will *logically* “remove” legacy-only structures from the OO build, but must keep compilation compatibility for FB2.5.

[Functions]
Complete Phase 12–15 by converting remaining legacy-dependent operations in the OO path to OO equivalents and restoring a correct legacy path for FB2.5.

New functions (C, new header `src/php_fbird_compat.h`):
- `fbird_api_mode fbird_detect_link_mode(const fbird_db_link* link);`
- `int fbird_require_oo(const fbird_db_link* link, const char* what);`
- `int fbird_require_legacy(const fbird_db_link* link, const char* what);`
- `void* fbird_get_attachment_checked(const fbird_db_link* link);`
- `void* fbird_get_itransaction_checked(const fbird_transaction* trans);`

Modified functions (exact names + changes):

1) `int _php_fbird_attach_db(char **args, size_t *len, zend_long *largs, void **db)` (firebird.c)
- Current behavior: always uses `fbc_connect()` and writes `*db = fbc_get_attachment(connection)`.
- Required changes:
  - If compiled with FB_API_VER >= 30: keep using `fbc_connect()`, but set `*db` to NULL (or to a dedicated legacy handle if and only if we explicitly create one). Do **not** store `IAttachment*` into `isc_db_handle` slots.
  - If compiled with FB_API_VER < 30: use legacy `isc_attach_database()` path and set `*db` to `isc_db_handle`.

2) `_php_fbird_connect()` (firebird.c)
- After allocating `fbird_db_link`, set:
  - `ib_link->api_mode = FBIRD_API_MODE_OO` when connection created via OO.
  - `ib_link->api_mode = FBIRD_API_MODE_LEGACY` when legacy.

3) `int _php_fbird_def_trans(fbird_db_link *ib_link, fbird_transaction **trans)` (firebird.c)
- Ensure the created default transaction inherits `api_mode` from the link.
- FB2.5 legacy builds: must continue using `isc_start_transaction()`.

4) `static int _php_fbird_exec(...)` (fbird_query_exec.c)
- Replace hard-coded legacy denial:
  - Implement a correct legacy execution branch for legacy builds and/or legacy links.
- Remove dependence on `isc_dsql_sql_info()` for OO statements.
  - For OO DML affected rows: use `fbs_get_affected_records()` after `fbs_execute()`.
  - For OO statement type: already uses `fbs_get_type()` in `_php_fbird_set_query_info()`.

5) `int _php_fbird_set_query_info(fbird_query *ib_query)` (fbird_query_prepare.c)
- Ensure it never uses legacy calls for OO statements.
- If legacy mode: either use existing legacy query info logic (if still needed) or provide a parallel legacy implementation.

6) `PHP_FUNCTION(fbird_trans_info)` (firebird.c)
- Current: uses `isc_transaction_info()` unconditionally.
- Required: add OO branch when `trans->fbt_transaction != NULL` using `ITransaction::getInfo()` via a new `fbt_get_info(...)` C-interop function, returning the same array keys.

Removed functions:
- None (public API remains stable). Internal legacy helpers may be conditionally compiled out under FB3+ once their call sites are migrated, but the FB2.5 build must retain them.

[Classes]
Use existing RAII wrappers for OO API, and add small missing “info/query” wrapper capabilities to eliminate remaining legacy dependencies.

New/extended classes (C++):
- Extend `fb::StatementWrapper` (src/cpp/fb_statement.hpp)
  - Ensure it exposes:
    - `unsigned getType(IMaster*, ISC_STATUS*) noexcept;`
    - `ISC_UINT64 getAffectedRecords(IMaster*, ISC_STATUS*) noexcept;`
    - `bool free(ISC_STATUS*) noexcept;` (already implied by `fbs_free()` usage)
  - Ensure the methods do not leak/release metadata incorrectly and are compatible with FB 3.0–5.0.

- Add `fb::Transaction::getInfo(...)` support (src/cpp/fb_transaction.hpp)
  - New method:
    - `std::vector<unsigned char> getInfo(const unsigned char* items, unsigned itemsLength, unsigned bufferSizeHint);`
  - New C interop:
    - `int fbt_get_info(void* master_ptr, void* transaction_ptr, const unsigned char* items, unsigned items_len, unsigned char* out_buf, unsigned out_buf_len, ISC_STATUS* status_vector);`

No existing classes should be removed in this phase.

[Dependencies]
No new third-party dependencies are required.

Build-time conditional dependencies:
- **FB 2.5 build:** requires only legacy Firebird headers and C compiler.
- **FB 3.0+ build:** requires Firebird OO API headers (`firebird/Interface.h`) and a C++17 compiler; this is already in place in current Docker images.

Optional tooling:
- Keep existing `clang-tidy`, `cppcheck`, PHPStan, and `phpcs` configurations unchanged.

[Testing]
Run the full Docker matrix and enforce that SKIPIF never emits warnings.

1) Matrix axes
- PHP containers: `php81-dev`, `php82-dev`, `php83-dev`, `php84-dev`, `php85-dev`, `php85-fb5-dev`.
- Firebird servers: `firebird25`, `firebird30`, `firebird40`, `firebird50`.

2) Required validations
- For each (PHP, server) combination:
  - `scripts/container/build.sh`
  - `scripts/container/test.sh` (entire `tests/` suite)
  - No BORK in SKIPIF.

3) Test additions/adjustments
- Add a focused regression test verifying that `fbird_query()` works during SKIPIF/init context (no warnings) by indirectly validating that `tests/firebird.inc:init_db()` completes without notices/warnings.
- Update/verify `tests/firebird.inc` database path strategy works for all servers, especially FB2.5.

[Implementation Order]
Execute changes in a dependency-safe sequence that restores test execution early and then completes the Phase 12–15 cleanup.

1. Introduce `src/php_fbird_compat.h` and add `api_mode` fields to `fbird_db_link`, `fbird_transaction`, `fbird_query` (no behavioral change yet).
2. Fix `_php_fbird_attach_db()` and `_php_fbird_connect()` to correctly set mode and to stop casting `IAttachment*` into `isc_db_handle` (OO mode only).
3. Restore dual-mode execution in `fbird_query_exec.c`:
   - Remove unconditional legacy denial.
   - Remove OO dependence on `isc_dsql_sql_info()` by using `fbs_get_affected_records()`.
4. Restore dual-mode preparation in `fbird_query_prepare.c` (OO when available, legacy otherwise).
5. Migrate `fbird_trans_info()` to use OO getInfo when in OO mode.
6. Audit remaining call sites that can still reach legacy `isc_*` functions while in OO mode (blobs, arrays, service, events, inspection) and migrate/gate them.
7. Update `scripts/host/test_matrix.sh` to run full matrix (PHP × server) with a default “fast” mode.
8. Run full matrix; fix any server-version-specific issues (especially FB2.5) until all combinations pass without BORK.
9. Final Phase 14 cleanup: consolidate conditional compilation so FB2.5 legacy is isolated, and FB3+ OO build does not accidentally compile legacy-only paths.
10. Final Phase 15 verification: rerun full matrix + static analysis (cppcheck/clang-tidy if available in containers) and document results.
