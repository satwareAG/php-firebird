---
description: >-
  P0 build fix: create firebird_legacy_wrappers.h/.c to bridge
  fbird_*.c files to removed firebird_utils.h v1 API signatures.
tags: [p0, build-fix, issue-132, v10-migration]
priority: 1
---

# Spec #132: Legacy Wrappers for v2 API Migration

## Goal

Unblock the build on `satware-main` by providing C wrapper functions that restore
the v1 API signatures removed from `firebird_utils.h` during the v2 migration.

## Success Criteria

- [ ] `docker compose run --rm php83-dev /ext/scripts/build.sh` exits 0
- [ ] `firebird_legacy_wrappers.h` declares all wrapper functions
- [ ] `firebird_legacy_wrappers.c` implements all wrappers calling `isc_*` directly
- [ ] `config.m4` includes `firebird_legacy_wrappers.c` in PHP_NEW_EXTENSION
- [ ] Zero modifications to `firebird_utils.h` or `firebird_utils.cpp`
- [ ] All 247 existing tests pass after build fix

## Current State

- Build FAILS with implicit function declarations in `fbird_classes.c`
- 15+ `.c` files reference removed v1 functions (fbb_\*, fbsvc_\*, fbe_\*, fbm_\*, fbs_\*, fbxpb_\*, fbu_\*_tz)
- Branch `fix/v10-p0-legacy-handle-events` has Step 1 complete (php_fbird_includes.h)

## Target State

- All `.c` files compile against legacy wrappers
- Wrappers are thin C functions calling `isc_*` C API directly
- No behavioral change; wrappers are 1:1 pass-through to old signatures

## Wrapper Categories

### Blob (fbb_\*)

| Wrapper | Calls |
|---------|-------|
| `fbb_create` | `isc_create_blob2` |
| `fbb_open` | `isc_open_blob2` |
| `fbb_close` | `isc_close_blob` |
| `fbb_free` | `isc_close_blob` (alias) |
| `fbb_put_segment` | `isc_put_segment` |
| `fbb_get_segment` | `isc_get_segment` |
| `fbb_cancel` | `isc_cancel_blob` |
| `fbb_get_info` | `isc_blob_info` |
| `fbb_seek` | `isc_blob_seek_params` |
| `fbb_get_blob_id` | Read from `isc_blob_handle` |

Pattern: `(fbc_master_t* master, ..., ISC_STATUS* status)` - master unused but kept for compat.
`fbb_blob_t` is opaque wrapper around `isc_blob_handle`.

**Optimization Note**: Firebird 4.0+ provides `isc_blob_set_data` and `isc_blob_get_data` for streaming. v10.1 should consider migrating `fbb_put_segment` and `fbb_get_segment` to these for improved performance when `FB_API_VER >= 40`.

### Service (fbsvc_\*)

| Wrapper | Calls |
|---------|-------|
| `fbsvc_attach` | `isc_service_attach` |
| `fbsvc_detach` | `isc_service_detach` |
| `fbsvc_free` | `isc_service_detach` (alias) |
| `fbsvc_start` | `isc_service_start` |
| `fbsvc_query` | `isc_service_query` |
| `fbsvc_is_attached` | Handle null check |

### Legacy Accessors

| Wrapper | Returns |
|---------|---------|
| `fbc_get_attachment` | `void*` cast of attachment |
| `fbt_get_handle` | `void*` cast of transaction |

### Event (fbe_\*)

| Wrapper | Calls |
|---------|-------|
| `fbe_event_free` | `free()` |
| `fbe_cancel` | `isc_cancel_events` |
| `fbe_free` | `free()` |
| `fbe_event_block` | `isc_event_block` (malloc'd buffer) |
| `fbe_wait_for_event_oo` | `isc_wait_for_event` |
| `fbe_event_counts` | `isc_event_counts` |

### Metadata (fbm_\*)

| Wrapper | Reads from |
|---------|------------|
| `fbm_get_count` | PHP internal struct (NOT XSQLDA) |
| `fbm_get_type` | PHP internal struct |
| `fbm_get_offset` | PHP internal struct |
| `fbm_get_null_offset` | PHP internal struct |
| `fbm_get_length` | PHP internal struct |
| `fbm_get_scale` | PHP internal struct |
| `fbm_get_subtype` | PHP internal struct |
| `fbm_get_alias` | PHP internal struct |
| `fbm_get_message_length` | PHP internal struct |
| `fbm_release` | `free()` |

**CRITICAL**: Read `fbird_metadata.c` first. The `void* metadata` parameter points to a
PHP-internal struct, not a Firebird native XSQLDA.

### Statement (fbs_\*)

| Wrapper | Implementation |
|---------|---------------|
| `fbs_get_output_count` | Read from stmt internal struct |
| `fbs_get_input_count` | Read from stmt internal struct |
| `fbs_is_cursor_open` | Read from stmt internal struct |
| `fbs_fetch_first/last/absolute/relative/prior` | `isc_dsql_fetch` with scroll options |
| `fbs_execute_singleton_int64` | prepare + execute + fetch + get int64 + close |

### TPB (fbxpb_\*)

| Wrapper | Implementation |
|---------|---------------|
| `fbxpb_build_tpb` | Parse comma-separated TPB string, build binary buffer |
| `fbxpb_free_tpb` | `free()` |

### TZ (fbu_\*_tz)

| Wrapper | Implementation |
|---------|---------------|
| `fbu_encode_time_tz` | Passthrough to non-TZ version |
| `fbu_encode_timestamp_tz` | Passthrough to non-TZ version |
| `fbu_decode_time_tz` | Passthrough to non-TZ version |
| `fbu_decode_timestamp_tz` | Passthrough to non-TZ version |

## Affected Files

### New Files
- `firebird_legacy_wrappers.h` - declarations
- `firebird_legacy_wrappers.c` - implementations

### Modified Files
- `config.m4` - add to PHP_NEW_EXTENSION
- `fbird_connection.c` (line 9) - remove `#include "src/cpp/fb_bridge.h"`
- `fbird_transaction.c` (line 85) - remove duplicate `#define TPB_MAX_SIZE`
- `fbird_service.c` (line 69) - fix `LE_SCVH` -> `LE_SVC`
- 15 additional `.c` files - add `#include "firebird_legacy_wrappers.h"` and apply migration patterns

## Risks

| Risk | Mitigation |
|------|------------|
| Metadata wrappers read wrong struct | READ fbird_metadata.c first; verify struct layout |
| Scroll fetch wrappers incomplete | Check firebird_utils.h for existing scroll functions |
| Wrapper ABI mismatch | Match exact v1 signatures from git blame |
| Circular include | Include guards; wrappers only need isc_api.h |

## Test Strategy

1. Build succeeds: `docker compose run --rm php83-dev /ext/scripts/build.sh`
2. Full test suite passes: `docker compose run --rm php83-dev /ext/scripts/test.sh`
3. No new warnings with `-Wall -Wextra`
4. Valgrind clean on blob/service/event tests