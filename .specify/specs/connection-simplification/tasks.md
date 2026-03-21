# Tasks: Connection Simplification — Remove Legacy Dual-Handle Architecture

**Issue**: Connection simplification (internal)
**Plan**: `.specify/specs/connection-simplification/plan.md`
**Date**: 2026-03-21

> Format: `- [ ] [TaskID] [Priority] Description → file-path`
> Priority: P0 (blocking), P1 (required), P2 (nice-to-have)
> Parallel tasks: mark with [P] — can run concurrently

---

## Phase 0: Setup & Prerequisites

- [ ] [T01] [P0] Create branch `feat/connection-simplification` from `satware-main`
- [ ] [T02] [P0] Verify Docker environment works: `docker compose run --rm php83-dev make test`

---

## Phase 1: Connection Struct

### Tests (RED)

- [ ] [T10] [P0] Create test `tests/connection_oo_only.phpt` — verify connect/disconnect/query cycle works (will pass initially, serves as regression guard) → `tests/connection_oo_only.phpt`
- [ ] [T11] [P0] Verify existing `tests/fbird_connect_001.phpt`, `tests/fbird_pconnect_001.phpt` pass before changes

### Implementation (≤200 LOC each)

- [ ] [T12] [P0] Add `fbird_get_attachment(link)` inline helper to `src/php_fbird_compat.h` — extracts `IAttachment*` via `fbc_get_attachment(link->fbc_connection)` → `src/php_fbird_compat.h`
  - Commit: `refactor(compat): add fbird_get_attachment() inline helper`
  - Est. ~30 LOC changed

- [ ] [T13] [P0] Rewrite `_php_fbird_attach_db()` signature to return `void*` (connection pointer) directly; remove status-vector smuggling hack (lines 260-262, 411-412); update `_php_fbird_connect()` caller → `fbird_connection.c`
  - Commit: `refactor(connection): return connection pointer directly from attach`
  - Est. ~60 LOC changed

- [ ] [T14] [P0] Replace all `handle.db` usages in `fbird_connection.c` with `fbird_get_attachment()` or `link->fbc_connection` → `fbird_connection.c`
  - Commit: `refactor(connection): replace handle.db usages in fbird_connection.c`
  - Est. ~40 LOC changed

- [ ] [T15] [P0] Replace all `handle.db` / `ib_link->handle.db` usages in `firebird.c`, `fbird_inspection.c`, `fbird_query_exec.c`, `fbird_events.c` with `fbird_get_attachment()` → multiple files
  - Commit: `refactor(connection): replace handle.db usages across extension`
  - Est. ~80 LOC changed

- [ ] [T16] [P0] Remove `fb_safe_handle handle` field from `fbird_db_link` struct; remove `handle.ptr` assignment → `php_fbird_includes.h`, `fbird_connection.c`
  - Commit: `refactor(connection): remove legacy handle from fbird_db_link`
  - Est. ~30 LOC changed

### Validation

- [ ] [T17] [P0] Run full test suite in Docker → all pass (GREEN)
- [ ] [T18] [P0] Run sanitizers → `docker compose run --rm php83-dev /ext/scripts/run-sanitizer.sh`

---

## Phase 2: Persistent Ping

### Tests (RED)

- [ ] [T20] [P0] Verify `tests/fbird_pconnect_001.phpt` and `tests/fbird_pconnect_limit.phpt` pass before changes

### Implementation

- [ ] [T21] [P0] Add `fbc_ping()` wrapper in `src/cpp/fb_connection.hpp` — calls `IAttachment::getInfo()` with `isc_info_base_level`, returns 1 if alive, 0 if dead → `src/cpp/fb_connection.hpp`
  - Commit: `feat(connection): add fbc_ping() OO wrapper for connection liveness check`
  - Est. ~50 LOC changed

- [ ] [T22] [P0] Replace `isc_database_info()` call in `_php_fbird_connect()` persistent-reuse check (line 360) with `fbc_ping(link->fbc_connection)` → `fbird_connection.c`
  - Commit: `refactor(connection): replace isc_database_info with fbc_ping for persistent check`
  - Est. ~20 LOC changed

### Validation

- [ ] [T23] [P0] Run persistent connection tests + full suite → all pass
- [ ] [T24] [P0] Run sanitizers

---

## Phase 3: Transaction Struct

### Tests (RED)

- [ ] [T30] [P0] Verify existing `tests/fbird_transaction_*.phpt` tests pass before changes

### Implementation

- [ ] [T31] [P0] Add `fbird_get_transaction(trans)` inline helper to `src/php_fbird_compat.h` — extracts `ITransaction*` via `fbt_get_handle(trans->fbt_transaction)` → `src/php_fbird_compat.h`
  - Commit: `refactor(compat): add fbird_get_transaction() inline helper`
  - Est. ~30 LOC changed

- [ ] [T32] [P0] Replace all `handle.tr` usages in `fbird_transaction.c`, `fbird_query_exec.c`, `fbird_query_bind.c`, `firebird.c` with `fbird_get_transaction()` → multiple files
  - Commit: `refactor(transaction): replace handle.tr usages across extension`
  - Est. ~80 LOC changed

- [ ] [T33] [P0] Remove `fb_safe_handle handle` field from `fbird_transaction` struct → `php_fbird_includes.h`
  - Commit: `refactor(transaction): remove legacy handle from fbird_transaction`
  - Est. ~20 LOC changed

### Validation

- [ ] [T34] [P0] Run full test suite → all pass
- [ ] [T35] [P0] Run sanitizers

---

## Phase 4: Blob Struct

### Tests (RED)

- [ ] [T40] [P0] Verify existing `tests/fbird_blob_*.phpt` tests pass before changes

### Implementation

- [ ] [T41] [P0] Add `fbird_get_blob(blob)` inline helper to `src/php_fbird_compat.h` — extracts `IBlob*` via `fbb_get_handle(blob->fbb_blob)` → `src/php_fbird_compat.h`
  - Commit: `refactor(compat): add fbird_get_blob() inline helper`
  - Est. ~30 LOC changed

- [ ] [T42] [P0] Replace all `bl_handle.blob` usages in `fbird_blobs.c` with `fbird_get_blob()` or `blob->fbb_blob` → `fbird_blobs.c`
  - Commit: `refactor(blob): replace bl_handle.blob usages in fbird_blobs.c`
  - Est. ~60 LOC changed

- [ ] [T43] [P0] Remove `fb_safe_handle bl_handle` field from `fbird_blob` struct → `php_fbird_includes.h`
  - Commit: `refactor(blob): remove legacy bl_handle from fbird_blob`
  - Est. ~20 LOC changed

### Validation

- [ ] [T44] [P0] Run full test suite → all pass
- [ ] [T45] [P0] Run sanitizers

---

## Phase 5: Events

### Tests (RED)

- [ ] [T50] [P0] Verify existing `tests/fbird_event_*.phpt` tests pass before changes
- [ ] [T51] [P1] Create `tests/fbird_event_oo.phpt` — test event registration and firing via OO path → `tests/fbird_event_oo.phpt`

### Implementation

- [ ] [T52] [P0] Create `src/cpp/fb_events.hpp` — C++ wrapper with C-callable `fbe_*` functions:
  - `fbe_que_events()` — wraps `IAttachment::queEvents()`
  - `fbe_create_event_block()` — manual EPB construction
  - `fbe_decode_events()` — wraps event count decoding
  - `fbe_release_events()` — wraps `IEvents::release()`
  - Commit: `feat(events): add OO event API wrapper in fb_events.hpp`
  - Est. ~150 LOC

- [ ] [T53] [P0] Replace `isc_wait_for_event()`, `isc_event_block()`, `isc_event_counts()`, `isc_free()` in `fbird_events.c` with `fbe_*` wrappers → `fbird_events.c`
  - Commit: `refactor(events): replace legacy isc event functions with OO API`
  - Est. ~120 LOC changed

### Validation

- [ ] [T54] [P0] Run event tests + full suite → all pass
- [ ] [T55] [P0] Run sanitizers

---

## Phase 6: Arrays

### Tests (RED)

- [ ] [T60] [P0] Verify existing array tests pass before changes

### Implementation

- [ ] [T61] [P0] Add C-callable OO wrappers for array operations in `firebird_utils.cpp`:
  - `fba_get_slice()` — wraps `IAttachment::getSlice()`
  - `fba_put_slice()` — wraps `IAttachment::putSlice()`
  - `fba_lookup_bounds()` — query `RDB$FIELD_DIMENSIONS` system table
  - Commit: `feat(array): add OO array slice wrappers`
  - Est. ~150 LOC

- [ ] [T62] [P0] Replace `isc_array_lookup_bounds()`, `isc_array_get_slice()`, `isc_array_put_slice()` in `fbird_query_array.c` with `fba_*` wrappers; update `_php_fbird_alloc_array()` signature to remove `fb_safe_handle` params → `fbird_query_array.c`, `php_fbird_query_array.h`
  - Commit: `refactor(array): replace legacy isc_array functions with OO API`
  - Est. ~120 LOC changed

### Validation

- [ ] [T63] [P0] Run array tests + full suite → all pass
- [ ] [T64] [P0] Run sanitizers

---

## Phase 7: Cleanup

### Implementation

- [ ] [T70] [P0] Remove `fb_safe_handle` union typedef from `php_fbird_includes.h`; remove from `fbird_result_set.stmt` if no longer needed (or leave with TODO for statement spec) → `php_fbird_includes.h`
  - Commit: `refactor(cleanup): remove fb_safe_handle union`
  - Est. ~30 LOC changed
  - Note: `fbird_result_set.stmt` may retain `isc_stmt_handle` directly if statement OO migration is deferred

- [ ] [T71] [P0] Simplify `src/php_fbird_compat.h` — remove dual-mode branches, collapse to OO-only checks; remove dead code → `src/php_fbird_compat.h`
  - Commit: `refactor(cleanup): simplify compat shims to OO-only`
  - Est. ~60 LOC changed

- [ ] [T72] [P1] Verify zero `isc_*()` function calls remain: `grep -rn 'isc_[a-z].*(' *.c` → zero matches (excluding comments, strings, type constants)

### Final Validation

- [ ] [T73] [P0] Run full test suite across Docker matrix (PHP 8.2-8.5 × FB 3.0-5.0)
- [ ] [T74] [P0] Run coverage → verify ≥80% on all touched files
- [ ] [T75] [P0] Run sanitizers (ASan + UBSan)
- [ ] [T76] [P1] Run Valgrind → no definite leaks

---

## Phase 8: PR & Merge

- [ ] [T80] [P0] Push branch, open PR with spec reference
- [ ] [T81] [P0] Verify all CI checks pass (12 matrix combinations)
- [ ] [T82] [P0] Merge PR → `satware-main`

---

## Progress Tracking

| Phase | Tasks | Done | Status |
|-------|-------|------|--------|
| Setup | 2 | 0 | ⬜ |
| Phase 1: Connection | 9 | 0 | ⬜ |
| Phase 2: Persistent Ping | 5 | 0 | ⬜ |
| Phase 3: Transaction | 6 | 0 | ⬜ |
| Phase 4: Blob | 6 | 0 | ⬜ |
| Phase 5: Events | 6 | 0 | ⬜ |
| Phase 6: Arrays | 5 | 0 | ⬜ |
| Phase 7: Cleanup | 7 | 0 | ⬜ |
| Phase 8: PR & Merge | 3 | 0 | ⬜ |
| **Total** | **49** | **0** | ⬜ |
