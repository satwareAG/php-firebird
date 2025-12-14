# Full Suite Summary

Date: 2025-12-14

Environment:
- Container: `docker/php/Dockerfile-8.4` (`php84-dev`)
- PHP: 8.4.15
- Extension: firebird 1.0.0

## Transaction Domain (focus)

### Status
- ✅ Transaction tests now green, including TPB array options + `fbird_trans_info()`.

### Fixes applied
- `fbird_trans_info()` now queries transaction info using Firebird 3+ OO API `ITransaction::getInfo()` via `fbt_get_info(...)`.
- Removed legacy `isc_transaction_info()` fallback path (legacy code paths are never allowed).
- Fixed build break in `fbt_get_info()` by copying status vectors using internal helper `copy_status_vector(...)`.

### Verified
Ran in `php84-dev`:
- `tests/trans_tpb_001.phpt` ✅
- Transaction subset (20 tests):
  - `tests/fbird_trans_002..014.phpt`
  - `tests/trans_tpb_001.phpt`, `tests/trans_tpb_reservation.phpt`
  - `tests/fbird_commit_001.phpt`, `tests/fbird_rollback_001.phpt`, `tests/fbird_rollback_002.phpt`
  - `tests/savepoint_001.phpt`, `tests/savepoint_error_001.phpt`
  ✅ all passed

## Full Suite (php84-dev)

Command: `/ext/scripts/container/test.sh`

Result:
- Total: 102
- Passed: 81
- Failed: 17
- Skipped: 4

### Failing tests
- `tests/003.phpt` (misc sql types)
- `tests/004.phpt` (BLOB test)
- `tests/006.phpt` (binding)
- `tests/007.phpt` (array handling)
- `tests/007_iso_char.phpt`
- `tests/007_iso_integer.phpt`
- `tests/007_iso_varchar10.phpt`
- `tests/007_iso_varchar1000.phpt`
- `tests/008.phpt` (event handling basic API)
- `tests/blob_stream_chunked_write.phpt`
- `tests/datatype_char_utf8.phpt`
- `tests/execute_safety_001.phpt`
- `tests/fbird_name_result_001.phpt`
- `tests/long_names_001.phpt`
- `tests/migration_001.phpt`
- `tests/proc-001.phpt`
- `tests/returning_001.phpt`

## Next Domain Proposal

Pick one (I recommend doing these sequentially):

1. **Array handling (Phase 9)**
   - `tests/007*.phpt`
   - Likely still using legacy `isc_array_*` paths or incorrect OO slice usage.

2. **BLOB streaming & chunked write**
   - `tests/004.phpt`, `tests/blob_stream_chunked_write.phpt`
   - Focus on stream wrapper integration with `IBlob` wrappers.

3. **Event handling basic API**
   - `tests/008.phpt`
   - Likely legacy synchronous path vs OO queue/cancel interactions.

4. **Query metadata / name result / long identifiers / RETURNING / procedures**
   - `tests/fbird_name_result_001.phpt`, `tests/long_names_001.phpt`, `tests/proc-001.phpt`, `tests/returning_001.phpt`
   - Probably in `fbird_metadata.c` / `fbird_query_*`.
