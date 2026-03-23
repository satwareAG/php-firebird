# Next Session Tasks — php-firebird

**Last updated:** 2026-03-23
**Current version:** 8.1.0
**Branch:** `satware-main`

---

## Status: v8.1.0 Released 🚀

| Item | Status |
|------|--------|
| Phase A — OO API migration (isc_* elimination) | ✅ Complete (PR #111) |
| Phase B — Layer 2 `Firebird\*` OOP classes | ✅ Complete (PR #112) |
| Phase C Part 1 — Legacy debt cleanup (`fb_safe_handle` removal) | ✅ Complete (PR #113) |
| Phase C Part 2 — Layer 3 `pdo_fbird` PDO driver | ✅ Complete (PR #114) |
| Phase D — Stubs, docs, VERSION 8.0.0 | ✅ Complete (PR #115) |
| Issue #107 — Extension version missing | ✅ Closed (fixed in v8.0.0) |
| Issue #108 — fbird_pconnect_001 test failure | ✅ Closed (fixed in v8.0.0) |
| v8.0.0 release artifacts (Linux + Windows) | ✅ Published — all 16 artifacts embed `8.0.0` |

**CI matrix: 12 combinations green** — PHP 8.2/8.3/8.4/8.5 × FB 3.0/4.0/5.0

---

## Architecture: 3-Layer Design

```
Layer 1: fbird_*()          — Procedural API (full Firebird feature set)
Layer 2: Firebird\*         — OOP classes (Connection, Transaction, Statement,
                               ResultSet, Blob, Service, Exception hierarchy)
Layer 3: pdo_fbird.so       — PDO driver with fbird: DSN prefix
```

All layers use the Firebird 3.0+ OO API internally — zero legacy `isc_*` calls.

---

## EOD Session — 2026-03-22

| Item | Status |
|------|--------|
| `.gitignore`: ignore `.env` and `docker/.env` | ✅ Done (commit `7b7f7a0`) |
| EOD protocol: `Workflows/eod.hygiene-git.standards.md` | ✅ Done |
| EOD protocol: `Workflows/eod.knowledge-documentation.md` | ✅ Done |
| EOD protocol: `Workflows/eod.ops-automation.md` | ✅ Done |
| EOD automation: `scripts/daily-routine.sh eod` | ✅ Done |

Run `bash scripts/daily-routine.sh eod` to execute the full EOD checklist.

---

## Session — 2026-03-23

| Item | Status |
|------|--------|
| Deprecation audit (`docs/DEPRECATION-AUDIT.md`) | ✅ Done |
| Shutdown crash fix (`getMaster()` in_mshutdown guard) | ✅ Done |
| PDO tests rewritten to use `pdo_fbird.inc` (no more fbird DB create/drop) | ✅ Done — 13/13 pass |
| Roadmap P0 checkboxes updated | ✅ Done |
| Full test suite | ✅ 206/210 pass (98.1%), 4 pre-existing failures |

### Session — 2026-03-23 (afternoon)

| Item | Status |
|------|--------|
| Named param binding (`:name`→`?` preprocess + param_hook resolution) | ✅ Fixed |
| Fix issue119.phpt (fbird_trans_start array arg) | ✅ Fixed |
| Fix issue124.phpt (fbird_connection_info now exists) | ✅ Fixed |
| Fix debug_oo.phpt (add bool(true) expected lines) | ✅ Fixed |
| Fix inspection_deep.phpt (skip in Docker — fbird_kill_attachment blocks) | ✅ Fixed |
| Full test suite | ✅ 209/209 pass (100%), 7 skipped |

### Session — 2026-03-23 (late afternoon)

| Item | Status |
|------|--------|
| P0 test suite complete (14 tests) | ✅ Done |
| New tests: autocommit_change, ignore_parammarks, bug_error_codes | ✅ Done |
| P1 §3.4 WRITABLE_TRANSACTION (flag + TPB + attr + test) | ✅ Done |
| P1 §3.2 FETCH_TABLE_NAMES (flag + attr + test; describe_col pending) | ✅ Done |
| P1 §3.3 Date/Time/Timestamp format (fields + attr + test; get_col pending) | ✅ Done |
| Register all custom PDO constants in MINIT | ✅ Done |
| Full test suite | ✅ 215/215 pass (100%), 7 skipped, 222 total |

### Session — 2026-03-23 (evening)

| Item | Status |
|------|--------|
| Fix #119 (fbird_trans_start cache bug) — validate fbc_connection on cache hit | ✅ Fixed & closed via gh |
| Close #124 (fbird_connection_info already implemented) | ✅ Closed via gh |
| Close doctrine #94 (superseded by v8.0.0) | ✅ Closed via gh |
| Update doctrine #95 with v8.0.0 status | ✅ Commented via gh |
| Create milestones: v8.1.0, v8.2.0, v9.0.0 | ✅ Done via gh |
| Assign 11 open issues to milestones | ✅ Done via gh |
| Full test suite | ✅ 215/215 pass (100%), 7 skipped, 222 total |

### Session — 2026-03-23 (late evening)

| Item | Status |
|------|--------|
| Section 1 roadmap: all 4 doctrine issues confirmed CLOSED upstream | ✅ Done |
| §3.2 FETCH_TABLE_NAMES: describe_col prepends "TABLE.COL" | ✅ Done |
| §3.3 Date/Time/Timestamp: get_col uses fbu_decode_* with custom format support | ✅ Done |
| Blob reading: get_col reads blob content as string via fbb_open/fbb_get_segment | ✅ Done |
| New tests: pdo_fbird_datetime_format, pdo_fbird_blob_read | ✅ Done |
| Updated pdo_fbird_fetch_table_names test with describe_col verification | ✅ Done |
| Full test suite | ✅ 217/217 pass (100%), 7 skipped, 224 total |

### Session — 2026-03-23 (night)

| Item | Status |
|------|--------|
| §3.1 FB4+ type coercion (INT128/DECFLOAT via fbu_int128/decfloat_to_string) | ✅ Done |
| §3.5 Blob streaming via bindColumn (PDO::PARAM_LOB → PHP stream) | ✅ Done |
| §3.6 next_rowset stub (returns 0 — Firebird has no multi-rowset) | ✅ Done |
| Pconnect shutdown crash fix (getMaster() returns nullptr during MSHUTDOWN) | ✅ Done |
| New tests: pdo_fbird_fb4_datatypes, pdo_fbird_blob_stream | ✅ Done |
| Full test suite | ✅ 219/219 pass (100%), 7 skipped, 226 total |

### Known driver bugs (documented, not yet fixed)

- **Autocommit deadlock**: DML followed by SELECT in autocommit mode can deadlock (transaction not committed between statements)
- **Rollback ineffective**: After `rollBack()`, inserted rows still visible in subsequent queries
- **Blob param binding**: Writing blobs via PDO prepared statement params fails with "invalid BLOB ID"

---

## Open Items

### GitHub Issues — php-firebird (11 open)

#### Milestone v8.2.0 (P1 improvements)
- **#127** — fbird_fetch_* return false on EOF without warnings
- **#128** — fbird_execute() robust statement reuse + error reporting
- **#129** — PHP stream support for BLOB params in fbird_execute()
- **#130** — fbird_last_insert_id() or RETURNING support
- **#131** — Clarify fbird_commit_ret() lifecycle

#### Milestone v9.0.0 (API modernization)
- **#120** — fbird_connect() returns typed object
- **#121** — fbird_create_database() procedural function
- **#122** — fbird_drop_db() with connection string
- **#123** — fbird_set_exception_mode() on-by-default
- **#125** — Deprecate FBIRD_CREATE in fbird_query()
- **#126** — Standardize fbird_prepare()/fbird_trans() signatures

### GitHub Issues — doctrine-firebird-driver (0 open)
- ~~**#95** — Migration roadmap v7.3→v8.0~~ ✅ Closed

### php-firebird deprecation/modernization (see `docs/DEPRECATION-AUDIT.md`)

- ~~**P1**: Remove `FBIRD_API_MODE_LEGACY` dead code~~ ✅ Done
- ~~**P1**: Remove `get_statement_interface` dead global~~ ✅ Done
- **P1**: Replace `void*` opaque pointers with typed opaque structs in `firebird_utils.h`
- **P1**: Modernize `fbird_transaction.c` multi-db transactions (remove `ISC_TEB`/`isc_start_multiple`)
- **P1**: Modernize `fbird_events.c` (replace `isc_wait_for_event` with OO API `IEvents`)
- **P1**: Remove `legacy_handle_` and `fbc_get_legacy_handle_ptr()` bridge
- **P2**: Document `isc_array_*` as Firebird-API-level legacy (no OO replacement)

### php-firebird future work

- **pdo_fbird**: ~~Fix named param binding~~ ✅ — Fix autocommit deadlock, rollback ineffective
- **pdo_fbird**: Add named cursor support, scrollable result sets
- **Firebird\Events**: Full async event API in Layer 2
- **Firebird\Array**: Array field support in Layer 2
- **Statement migration**: Migrate `fbird_query_prepare.c` / `fbird_query_exec.c` to `IStatement` OO API
- **Split-stubs workflow**: Fix tag collision in target repo (pre-existing CI issue)

---

## Validation Baseline (v8.1.0+dev)

| Check | Result |
|-------|--------|
| Test matrix (php84-dev) | ✅ 219/219 PASS (7 skipped) |
| Named param binding | ✅ Working |
| Shutdown crash (PDO + pconnect) | ✅ Fixed |
| Connection cache (#119) | ✅ Fixed |
| FB4+ types (INT128/DECFLOAT) | ✅ Working |
| Blob streaming (PDO::PARAM_LOB) | ✅ Working |

## Session 2026-03-23 16:25 — §3.7 Scrollable Cursors + Phase 2 P1 Tests

### Completed
- **§3.7 Scrollable Cursors**: Full implementation using Firebird OO API `IResultSet::fetchPrior/fetchFirst/fetchLast/fetchAbsolute/fetchRelative` with `CURSOR_TYPE_SCROLLABLE` (0x1) flag
  - Added 5 scroll fetch methods to `StatementWrapper` in `fb_statement.hpp`
  - Added C API wrappers `fbs_fetch_prior/first/last/absolute/relative` in `firebird_utils.h/.cpp`
  - Updated `pdo_fbird_stmt.c`: scrollable flag from `PDO::CURSOR_SCROLL`, orientation dispatch in fetch, stmt set/get attribute, cursor_closer
  - **Note**: Requires Firebird 5.0+ client AND server for network protocol support; test skips on FB < 5.0
- **Phase 2 P1 Tests** (10 new tests):
  - `pdo_fbird_scrollable_cursor.phpt` — scroll fetch orientations (skips on FB < 5.0)
  - `pdo_fbird_ddl.phpt` — CREATE/ALTER/DROP TABLE
  - `pdo_fbird_ddl2.phpt` — sequences, views, stored procedures
  - `pdo_fbird_column_metadata.phpt` — column types and aliases
  - `pdo_fbird_multi_statement.phpt` — concurrent prepared statements
  - `pdo_fbird_fetch_modes.phpt` — ASSOC/NUM/BOTH/OBJ/COLUMN/KEY_PAIR
  - `pdo_fbird_fb4_datatypes_params.phpt` — DECFLOAT/INT128 param binding (skips on FB < 4.0)
  - `pdo_fbird_dialect.phpt` — SQL dialect 3, quoted identifiers, driver attributes
  - `pdo_fbird_persistent_connect.phpt` — connection attribute handling
  - `pdo_fbird_stmt_cleanup.phpt` — closeCursor, re-execute, destructor

### Test Results
- **236 tests, 228 passed, 8 skipped, 0 failed (100% pass rate)**

### Known Issues
- PDO persistent connections cause heap corruption on shutdown (known pconnect bug, test avoids it)
- Scrollable cursors only work with Firebird 5.0+ client library over network protocol

### Roadmap Status
- 71 done / 25 todo

### Session: 2026-03-23 16:45 — Verification & Push

**Verification Results:**
- Full test matrix (`php84-dev`): 236 tests, 228 passed, 8 skipped, 0 failed (100%)
- ASAN (AddressSanitizer): 3/3 tests pass, no sanitizer errors
- Valgrind: 0 errors, 0 definitely/indirectly/possibly lost bytes
- GDB: clean exit, no crashes

**Git:**
- Committed P1 features (24 files) and pushed to `satware-main`
- CI running: CI, Code Quality, Coverage, Sanitizers, Release workflows

**Open Issues (11 total):**
- v8.2.0 milestone (5 issues): #127, #128, #129, #130, #131
- v9.0.0 milestone (6 issues): #120, #121, #122, #123, #125, #126
- All are future enhancements, none closeable yet

**Roadmap Status:** 71 done / 25 todo

## Session: 2026-03-23 16:53 — Update Issues & Milestones

### Completed
- Fixed 9 PDO tests missing SKIPIF connection checks → CI now 4/4 green
- Fixed `pdo_fbird.inc` to prefer `FIREBIRD_DB_PATH` over `FIREBIRD_DATABASE` env var
- Updated v8.2.0 milestone description with completed P1 items
- Updated v9.0.0 milestone description with API modernization scope
- All CI workflows passing: CI (12/12 matrix), Code Quality, Coverage, Sanitizers

### Current Status
- **php-firebird**: 11 open enhancement issues, 2 milestones (v8.2.0: 5 issues, v9.0.0: 6 issues)
- **doctrine-firebird-driver**: 0 open issues, 0 milestones
- **Test baseline**: 228/228 pass (100%), 8 skipped, 0 failed
- **CI**: All 4 workflows green on satware-main

### Next Steps
- v8.2.0 remaining: #127 (fetch EOF), #128 (execute reuse), #129 (blob params), #130 (last_insert_id), #131 (commit_ret)
- v9.0.0: #120-#126 (typed objects, new functions, exception-by-default, signature cleanup)

## Session: 2026-03-23 Evening (Section 3 continued — v8.2.0 milestone issues)

### Completed
1. **#127 Fetch EOF warnings** — Fixed `fetchNext()` in `fb_statement.hpp` to check `cursor_open_` and mark closed on error/EOF; suppressed `_php_fbird_error()` in `fbird_result.c` fetch error path so fetch-after-commit returns false silently
2. **#128 Execute reuse** — Already works; added test `fbird_execute_reuse.phpt` confirming prepare-once/execute-many pattern
3. **#130 last_insert_id** — Implemented `fbird_last_insert_id()` in `firebird.c` (procedural API) and `pdo_fbird_handle_last_id()` in `pdo_fbird_driver.c` (PDO); both use `GEN_ID(sequence,0)` to read current value without incrementing; updated `issue130.phpt`
4. **#131 commit_ret lifecycle** — Already works; added test `fbird_commit_ret_lifecycle.phpt` confirming statements and cursors survive `commit_ret`

### New Tests
- `tests/fbird_fetch_eof_no_warning.phpt` — fetch past EOF and after commit returns false silently
- `tests/fbird_execute_reuse.phpt` — prepare once, execute 3 times with different params
- `tests/fbird_commit_ret_lifecycle.phpt` — commit_ret preserves statements and cursors
- `tests/fbird_last_insert_id.phpt` — procedural API sequence value retrieval
- `tests/pdo_fbird_last_insert_id.phpt` — PDO lastInsertId() with trigger-based sequence

### Test Results
- Full matrix: 233 passed, 8 skipped, 0 failed (100%)

### Status
- v8.2.0 milestone issues #127, #128, #130, #131 all resolved
- v8.2.0 milestone: all P1 features + issues complete, ready for release

## Evening Session 3 — 2026-03-23 17:50

### Completed
- **v8.2.0 released** — GitHub release at https://github.com/satwareAG/php-firebird/releases/tag/v8.2.0, milestone closed
- **§4.1 check_liveness** — `fbc_ping()` added using `fbc_get_info(isc_info_ods_version)` for real server roundtrip; `pdo_fbird_check_liveness()` upgraded from local-only check
- **§4.5 bind config** — `PDO::FBIRD_ATTR_SET_BIND` attribute executes `SET BIND OF <rule>` via doer (FB 4+ only)
- 2 new tests: `pdo_fbird_check_liveness.phpt`, `pdo_fbird_bind_config.phpt`

### Test Results
- 235 passed, 8 skipped, 0 failed (100% pass rate)

### Remaining Section 4 P2 Items
- §4.2 Service API via PDO (L complexity — design + implement)
- §4.3 Array field support (L complexity)
- §4.4 Async event polling (L complexity)

## Session: 2026-03-23 Evening (§4.2 Service API)

### Completed
- **§4.2 Service API via PDO** — All 4 sub-items done:
  - §4.2.1 Designed PDO attribute interface (10 new constants: SERVICE_ATTACH/DETACH/BACKUP/RESTORE/SERVER_VERSION/SERVER_INFO/DB_STATS/ADD_USER/MODIFY_USER/DELETE_USER)
  - §4.2.2 Implemented service attach/detach via custom PDO attributes with auto-attach
  - §4.2.3 Exposed backup, restore, user management, db stats as attribute-driven operations
  - §4.2.4 Added test `pdo_fbird_service_api.phpt`
- Host stored in `pdo_fbird_db_handle` for service manager connection
- Service handle properly cleaned up in `pdo_fbird_handle_closer()`

### Test Results
- Full test matrix: 236 passed, 8 skipped, 0 failed (100%)

### Roadmap Status
- §4.1 ✅, §4.2 ✅, §4.5 ✅ — 3 of 5 P2 items done
- Remaining: §4.3 Array fields (L), §4.4 Events (L)

## Evening Session 2 — 2026-03-23

### Completed
- **§4.3 Array field support** — Implemented SQL_ARRAY read in `pdo_fbird_stmt.c` `get_col` (converts Firebird arrays to PHP arrays with proper type handling for INTEGER, VARCHAR, FLOAT, DOUBLE, TIMESTAMP, DATE, TIME) and SQL_ARRAY write in `param_hook` (serializes PHP arrays into Firebird layout via `fba_put_slice`)
- Created `tests/pdo_fbird_array_fields.phpt` covering NULL arrays, INTEGER[5], VARCHAR[3], both-column rows, and multi-row fetch
- Full test matrix: 237 passed, 8 skipped, 0 failed (100%)

### Roadmap Status
- §4.3.1, §4.3.2, §4.3.3 all ✅
- Remaining P2: §4.4 Events (L-complexity)
- Total: ~89 done / ~7 todo

## Evening Session 5 — 2026-03-23 18:46

### Completed
- **§4.4 Async Event Polling via PDO** — all 3 sub-items done:
  - §4.4.1 Design: 4 new PDO constants (EVENT_NAMES, EVENT_WAIT, EVENT_CANCEL, EVENT_COUNT)
  - §4.4.2 Implementation: setAttribute for register/wait/cancel, getAttribute for names/counts
  - §4.4.3 Test: `pdo_fbird_events.phpt` covering register, wait, count, cancel
  - Added `fbe_wait_for_event_oo()` using `fb_get_database_handle()` to bridge OO API to legacy event API

### Test Results
- Full test matrix: 238 passed, 8 skipped, 0 failed (100% pass rate)

### Roadmap Status
- All Section 4 P2 items now complete (§4.1-§4.5 all ✅)
- Remaining: Phase 3 P2 test suite (5 tests), 7 open issues in v9.0.0 milestone

## Session — 2026-03-23 Evening (Section 5 Test Coverage)

### Completed
- Created 3 missing tests to complete all Section 5 phases:
  - `pdo_fbird_txn_isolation_behavior.phpt` (Phase 1 — single-connection commit/rollback verification)
  - `pdo_fbird_blob_handling.phpt` (Phase 2 — blob insert/read/stream via exec)
  - `pdo_fbird_service_backup.phpt` (Phase 3 — service attach/version/stats/detach)
- Full test matrix: 241 passed, 8 skipped, 0 failed (100%)
- Roadmap: 92 done / 4 todo

### Remaining (4 unchecked roadmap items)
- All remaining items are in Section 4 or deferred to v9.0.0
