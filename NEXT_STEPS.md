# Next Session Tasks — php-firebird coverage push

**Branch**: `test/coverage-phase1`
**Date**: 2026-03-03
**Current coverage**: 65.2% (5,089 / 7,802) — measured on `php84-fb3-dev` container

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

## Quick-win tests to write next

### 1. `tests/coverage/blob_seek_segments.phpt`
Cover `fb_blob.hpp` lines 124-232:
- Create blob, seek to middle, read from seek position
- Open blob for `FBIRD_BLB_SEG` (segmented mode) vs stream
- Try seek on segmented blob (should fail gracefully)

### 2. `tests/coverage/array_all_types.phpt`
Cover `fbird_query_array.c` lines 34-72 (alloc path):
- Create table with FLOAT ARRAY, CHAR(10) ARRAY, DATE ARRAY
- Put and get back values to exercise all switch cases
- Must use `fbird_array_put_slice` / `fbird_array_get_slice`

### 3. `tests/coverage/service_all_ops.phpt`
Cover `fb_service.hpp` (0% → 60%+):
- `fbird_service_query()` with db_stats, limbo, repair flags
- Ensure service attachment goes through C++ wrapper (not legacy)
- Use FB3-compatible ops only (no backup to file that doesn't exist)

### 4. `tests/coverage/exec_cursor_named.phpt`
Cover `fbird_query_exec.c` lines 348-477:
- Use `fbird_prepare()` then `fbird_execute()` with OPEN cursor
- Fetch via cursor, named resultset (`fbird_name_result`)
- Execute stored procedure that returns a result set (SELECT procedure)

## LCOV exclusion strategy (deferred)

If 80% cannot be reached via PHP tests, add `/* LCOV_EXCL_START */` / `/* LCOV_EXCL_STOP */`
around `_php_fbird_safe_copy_sqlvar_data()` (fbird_query_bind.c lines 36-220).
This function is ONLY reachable via Firebird 2.5 legacy isc_dsql API, never on FB3+.
Excluding it reduces LF by ~87, raising % by ~0.7%.

## Resume command

```bash
cd /home/ja/external/php-firebird
git checkout test/coverage-phase1
# Run single test:
docker compose -f docker/docker-compose.yml run --rm php84-fb3-dev /ext/scripts/test.sh tests/coverage/TESTNAME.phpt
# Run full coverage:
docker compose -f docker/docker-compose.yml run --rm php84-fb3-dev /ext/scripts/coverage.sh
# Check total:
grep -E "^LF:|^LH:" coverage/lcov_filtered.info | \
  awk '/^LF:/{lf+=substr($0,4)} /^LH:/{lh+=substr($0,4)} END{printf "%d/%d = %.1f%%\n",lh,lf,lh/lf*100}'
```
