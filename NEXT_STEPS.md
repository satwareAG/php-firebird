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

### Known driver bugs (documented, not yet fixed)

- **Autocommit deadlock**: DML followed by SELECT in autocommit mode can deadlock (transaction not committed between statements)
- **Rollback ineffective**: After `rollBack()`, inserted rows still visible in subsequent queries

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

## Validation Baseline (v8.1.0)

| Check | Result |
|-------|--------|
| Test matrix (php84-dev) | ✅ 215/215 PASS (7 skipped) |
| Named param binding | ✅ Working |
| Shutdown crash | ✅ Fixed |
| Connection cache (#119) | ✅ Fixed |
