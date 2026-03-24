# Next Steps - fix/v10-p0-legacy-handle-events

**Branch**: `fix/v10-p0-legacy-handle-events`
**Last commit**: `acc70d5` - Step 1 (php_fbird_includes.h)
**Build**: FAILS - fbird_classes.c implicit declarations

## Step 2: Create firebird_legacy_wrappers.h + firebird_legacy_wrappers.c

New files providing C wrappers for functions removed from firebird_utils.h v2 API.
These call isc_* directly to maintain old signatures expected by .c files.

### Wrappers needed (determined from build errors + code analysis):

**Blob (fbb_*)**: fbb_create, fbb_open, fbb_close, fbb_free, fbb_put_segment, fbb_get_segment, fbb_cancel, fbb_get_info, fbb_seek, fbb_get_blob_id
- Call isc_create_blob2, isc_open_blob2, isc_close_blob, isc_cancel_blob, isc_put_segment, isc_get_segment, isc_blob_info, isc_blob_seek_params
- Pattern: `(fbc_master_t*master, ..., ISC_STATUS*status)` - master unused but kept for compat
- fbb_blob_t is opaque wrapper around isc_blob_handle

**Service (fbsvc_*)**: fbsvc_attach, fbsvc_detach, fbsvc_free, fbsvc_start, fbsvc_query, fbsvc_is_attached
- Call isc_service_attach, isc_service_detach, isc_service_start, isc_service_query
- fbsvc_service_t is opaque wrapper around isc_svc_handle

**Legacy accessors**: fbc_get_attachment(conn)->void*, fbt_get_handle(trans)->void*
- Simple cast pass-through for opaque pointer compat

**Event (fbe_*)**: fbe_event_free, fbe_cancel, fbe_free, fbe_event_block, fbe_wait_for_event_oo, fbe_event_counts
- Call isc_event_block (malloc'd buffer), isc_event_counts, isc_cancel_events, isc_wait_for_event

**Metadata (fbm_*)**: fbm_get_count, fbm_get_type, fbm_get_offset, fbm_get_null_offset, fbm_get_length, fbm_get_scale, fbm_get_subtype, fbm_get_alias, fbm_get_message_length, fbm_release
- READ fbird_metadata.c FIRST - void*metadata likely points to PHP internal struct (fbird_metadata_t or similar), not Firebird native XSQLDA

**Statement (fbs_*)**: fbs_get_output_count, fbs_get_input_count, fbs_is_cursor_open, fbs_fetch_first/last/absolute/relative/prior, fbs_execute_singleton_int64
- Scroll API: check firebird_utils.h for existing functions or use isc_dsql_fetch with scroll options
- execute_singleton_int64: prepare, execute, fetch single row, get int64, close

**TPB (fbxpb_*)**: fbxpb_build_tpb, fbxpb_free_tpb
- Parse comma-separated TPB string ("read,write,wait"), build binary TPB buffer

**TZ (fbu_*_tz)**: fbu_encode_time_tz, fbu_encode_timestamp_tz, fbu_decode_time_tz, fbu_decode_timestamp_tz
- Simple passthrough without actual TZ support (encode/decode non-TZ version)

## Step 3: Update config.m4

Add `firebird_legacy_wrappers.c` to PHP_NEW_EXTENSION file list.

## Step 4: Fix simple one-line issues

| File | Line | Fix |
|------|------|-----|
| fbird_connection.c | 9 | Remove `#include "src/cpp/fb_bridge.h"` |
| fbird_transaction.c | 85 | Remove duplicate `#define TPB_MAX_SIZE 2048` |
| fbird_service.c | 69 | Fix `LE_SCVH` -> `LE_SVC` |

## Step 5: Fix all .c files (READ BEFORE MODIFY)

### Migration patterns:

| OLD | NEW |
|-----|-----|
| `void* att = fbc_get_attachment(conn);` | `fb_ptr_t _att = fbc_get_attachment_safe(conn); void* att = (void*)_att.p;` |
| `void* tr = fbt_get_handle(trans);` | `fb_opaque_transaction_t* tr = fbt_get_handle_safe((fb_ptr_t){(uintptr_t)((void*)trans)});` |
| `fbt_transaction_t* t = fbt_start(master, att, len, tpb, status);` | `fb_ptr_t _t = fbt_start_safe(master, (fb_ptr_t){(uintptr_t)att}, len, tpb, status); fbt_transaction_t* t = (fbt_transaction_t*)_t.p;` |
| `fba_lookup_bounds(...)` | `fba_lookup_bounds_oo(master, attach_oo, trans_oo, ...)` |
| `fba_get_slice(...)` | `fba_get_slice_oo(master, attach_oo, trans_oo, ...)` |
| `fba_put_slice(...)` | `fba_put_slice_oo(master, attach_oo, trans_oo, ...)` |
| `fbu_decode_timestamp(master, &ts, ...)` | `fbu_decode_timestamp(master, ts, ...)` (by value) |
| `fbu_int128_to_string(master, val, scale, buf, sz)` | `fbu_int128_to_string(val, scale, buf, sz)` (no master) |
| `fbbatch_execute(master, batch, trans, status)` | `fbbatch_execute(master, batch, status)` |
| `fbbatch_add(...)` | `fbbatch_add_row(...)` |
| `fbbatch_close(master, batch, status)` | `fbbatch_free(batch)` |
| `fbbatch_cancel(master, batch, status)` | `fbbatch_cancel(batch)` |
| `_php_fbird_error()` | `_php_fbird_error(THIS_ZVAL, __FILE__, __LINE__)` |

### Files to fix (in order):

1. **fbird_transaction.c** - fbc_get_attachment, fbt_start, fbt_get_handle, fbt_reconnect
2. **fbird_query_prepare.c** - fbc_get_attachment, fbt_get_handle, fbs_get_output_count/input_count, fbs_prepare return type
3. **fbird_query_exec.c** - fbc_get_attachment_64/fbt_start_64, extern "C" removal, was_result_once/out_array_cnt removal
4. **fbird_result.c** - all fb_ptr_t, fba_oo, fbu_decode_timestamp by-value, fbm_get_*, fbb_*, LL_LIT, fbird_array
5. **fbird_query_bind.c** - fba_oo, fb_ptr_t, fbb_*, fbm_get_*
6. **fbird_metadata.c** - METADATALENGTH define, fbm_get_alias
7. **fbird_batch.c** - Major rewrite (fbbatch_*)
8. **fbird_blobs.c** - add `#include "firebird_legacy_wrappers.h"` (signatures provided by wrappers)
9. **fbird_events.c** - all fbe_* calls (provided by legacy wrappers)
10. **fbird_service.c** - all fbsvc_* calls (provided by legacy wrappers)
11. **fbird_error.c** - MAX_ERRMSG, firebird_exception_ce, client_version fields
12. **firebird.c** - IB_DEF_DATE_FMT/TIME_FMT, firebird_exception_methods, blob_segment_size, get_master_interface, client_version, LE_* constants, fb_get_master_interface_t
13. **fbird_classes.c** - firebird_exception_ce, le_link/le_plink, fbc_connect return type, fbb_*, fbsvc_*, _fbird_get_link_from_obj signature fix
14. **fbird_query_array.c / php_fbird_query_array.h** - fbird_array type
15. **pdo_fbird/pdo_fbird_driver.c** - all safe functions, fbsvc_*, fbe_*, fbm_get_*, fbs_execute_singleton_int64, fbxpb_*
16. **pdo_fbird/pdo_fbird_stmt.c** - all safe functions, fbm_get_*, fbs_fetch_*, fbu_* signature changes, fba_oo, fbb_*

## Step 6: Build iteratively

```bash
docker compose -f docker/docker-compose.yml run --rm php83-dev /ext/scripts/build.sh
```

Fix errors one file at a time. Expected 5-10 build iterations.

## Key Constraints

- firebird_utils.h MUST NOT be modified
- firebird_utils.cpp MUST NOT be modified
- fbird_query struct does NOT have `was_result_once` or `out_array_cnt` members
- fba_array_lookup_bounds (non-oo) still exists in firebird_utils.h - keep it
- TPB_MAX_SIZE is already 128 in php_fbird_includes.h
- fbird_blobs.c uses OLD fbb_* signatures - provided by legacy wrappers
- Feature branch only, never satware-main