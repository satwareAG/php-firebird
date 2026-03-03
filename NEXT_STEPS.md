# Next Session Tasks — php-firebird coverage push

**Branch**: `satware-main` (all Phase 1 coverage tests merged)
**Date**: 2026-03-03
**Current coverage**: 65.2% (5,089 / 7,802) — measured on `php84-fb3-dev` container
**Version**: 7.0.0-rc.50

## Priority 1 — Blockers for 80%

```
File                         Gap    Approach
firebird_utils.cpp           281    Batch API → skip (FB4+ only, not reachable on FB3)
fbird_query_bind.c           146    87 lines dead on FB3 (legacy isc API); 59 coverable
src/cpp/fb_service.hpp       139    Service API tests (triggered via fbird_service_* PHP calls)
src/cpp/fb_blob.hpp          104    BlobWrapper::seek, segmented read, BLOBs opened without PHP close
fbird_query_array.c           90    Multi-dim arrays, CHAR/FLOAT/DATE types, isc_array_lookup_bounds
fbird_query_exec.c            88    Cursor operations, named resultset, EXECUTE PROCEDURE path
```

## Priority 2 — Near-80% polish

```
File                         Gap
src/cpp/fb_events.hpp         56    Event cancel, timeout — write event cancel test
src/cpp/fb_statement.hpp      48    Prepare/describe path (triggered when stmt executed)
fbird_events.c                45    poll timeout === 0, cancel during poll
src/cpp/fb_array.hpp          38    Multi-dim put, typed put operations
fbird_transaction.c           29    Limbo resolution, multi-DB transactions
fbird_inspection.c            27    Long attachment list, MON$STATEMENTS paths
firebird.c                    24    Remaining ibase_* alias paths
```

## Quick-win tests to write next (Phase 2)

### 1. `tests/coverage/blob_seek_segments.phpt` ← already exists, needs FB3 run validation
Cover `fb_blob.hpp` lines 124-232:
- Create blob, seek to middle, read from seek position
- Open blob for `FBIRD_BLB_SEG` (segmented mode) vs stream
- Try seek on segmented blob (should fail gracefully)

### 2. `tests/coverage/exec_cursor_named.phpt` ← NEW
Cover `fbird_query_exec.c` lines 348-477:
- Use `fbird_prepare()` then `fbird_execute()` with OPEN cursor
- Fetch via cursor, named resultset (`fbird_name_result`)
- Execute stored procedure that returns a result set (SELECT procedure)

### 3. `tests/coverage/service_all_ops.phpt` ← extend existing service tests
Cover `fb_service.hpp` (currently 0–20% → target 60%+):
- `fbird_service_query()` with db_stats, limbo, repair flags
- Use FB3-compatible ops only (no backup to file that doesn't exist)

## LCOV exclusion strategy (deferred fallback)

If 80% cannot be reached via PHP tests alone, add `/* LCOV_EXCL_START */` / `/* LCOV_EXCL_STOP */`
around `_php_fbird_safe_copy_sqlvar_data()` in `fbird_query_bind.c` lines 36-220.
This function is ONLY reachable via Firebird 2.5 legacy isc_dsql API, never on FB3+.
Excluding it reduces LF by ~87, raising % by ~0.7%.

## Active plans

- `implementation_plan.md` — Issue #57: Split `firebird.c` refactoring (P1, after 80% coverage)

## Resume commands

```bash
cd /home/ja/external/php-firebird
# Already on satware-main — no branch switch needed

# Run single test:
docker compose -f docker/docker-compose.yml run --rm php84-fb3-dev /ext/scripts/test.sh tests/coverage/TESTNAME.phpt

# Run full coverage:
docker compose -f docker/docker-compose.yml run --rm php84-fb3-dev /ext/scripts/coverage.sh

# Check total:
grep -E "^LF:|^LH:" coverage/lcov_filtered.info | \
  awk '/^LF:/{lf+=substr($0,4)} /^LH:/{lh+=substr($0,4)} END{printf "%d/%d = %.1f%%\n",lh,lf,lh/lf*100}'
```

## Open Issues (v7.0.0 Milestone)

| Issue | Title | Priority | Status |
|-------|-------|---------|--------|
| #58 | Code Coverage 80%+ gate | P0 | In Progress (65.2%) |
| #59 | Service API coverage (0%→80%) | P0 | In Progress |
| #61 | Array operations coverage (35.9%→80%) | P0 | In Progress |
| #62 | Parameter binding coverage (41.8%→80%) | P0 | In Progress |
| #63 | Final coverage validation ≥80% | P0 | Blocked on above |
| #57 | Source refactoring (firebird.c split) | P1 | Deferred to after 80% |
| #66 | Release rc.50 | P0 | Released (VERSION=7.0.0-rc.50) |
| #67 | Docs cleanup | P2 | Pending |
| #68 | Doctrine integration | P1 | After rc.final |
