# Spec: Connection Simplification — Remove Legacy Dual-Handle Architecture

**Issue**: Connection simplification (internal)
**Branch**: `feat/connection-simplification`
**Date**: 2026-03-21
**Status**: Draft

---

## Intent

> Eliminate the legacy `fb_safe_handle` union and all remaining `isc_*()` function
> calls from the extension, making `fbc_connection` / `fbt_transaction` / `fbb_blob`
> the sole connection/transaction/blob holders. This removes the dual-handle
> architecture that currently requires pointer-smuggling hacks, compatibility shims,
> and duplicated state — reducing maintenance burden and enabling future OO API
> features (batch DML, scrollable cursors, time zones) without legacy workarounds.

---

## Background

### Current Dual-Handle Architecture

The extension maintains **two parallel representations** for every database object:

1. **Legacy `fb_safe_handle` union** (`php_fbird_includes.h:115-119`):
   ```c
   typedef union {
       isc_db_handle db;
       isc_tr_handle tr;
       isc_stmt_handle stmt;
       isc_blob_handle blob;
   } fb_safe_handle;
   ```

2. **OO API wrappers** (C++ classes in `src/cpp/`):
   - `fbc_connection` (`fb_connection.hpp`) — wraps `IAttachment*`
   - `fbt_transaction` (`fb_transaction.hpp`) — wraps `ITransaction*`
   - `fbb_blob` (`fb_blob.hpp`) — wraps `IBlob*`

Each struct carries **both**:

| Struct | Legacy field | OO field | File |
|--------|-------------|----------|------|
| `fbird_db_link` | `handle.db` | `fbc_connection` | `php_fbird_includes.h:121-136` |
| `fbird_transaction` | `handle.tr` | `fbt_transaction` | `php_fbird_includes.h:139-147` |
| `fbird_blob` | `bl_handle.blob` | `fbb_blob` | `php_fbird_includes.h:155-163` |
| `fbird_result_set` | `stmt` (fb_safe_handle) | — (none yet) | `php_fbird_includes.h:222-224` |

### Legacy `handle.*` Usages

**66 usages** of `handle.` across **10 .c files**:

| File | Count | Primary usage |
|------|-------|---------------|
| `fbird_connection.c` | 5 | `handle.db` for attach/detach, persistent ping |
| `fbird_transaction.c` | 8 | `handle.tr` for start/commit/rollback |
| `fbird_blobs.c` | 10 | `bl_handle.blob` for create/open/close |
| `fbird_events.c` | 6 | `handle.db` for event registration |
| `fbird_inspection.c` | 4 | `handle.db` for `isc_database_info()` |
| `fbird_query_exec.c` | 8 | `handle.db`, `handle.tr` for execute |
| `fbird_query_bind.c` | 7 | `handle.db`, `handle.tr` for array binding |
| `fbird_result.c` | 5 | `stmt` handle for fetch |
| `fbird_service.c` | 3 | Service handle (separate, not in scope) |
| `firebird.c` | 10 | `handle.db` for gen_id, batch ops |

### Remaining `isc_*()` Function Calls

Despite the OO wrappers, **~616 `isc_*` references** remain (many are type constants
like `isc_info_*`). The **function calls** that must be replaced:

| Function | Files | OO API Replacement |
|----------|-------|--------------------|
| `isc_database_info()` | `fbird_connection.c`, `fbird_inspection.c` | `IAttachment::getInfo()` via `fbc_get_info()` |
| `isc_wait_for_event()` | `fbird_events.c` | `IAttachment::queEvents()` via new `fbe_*` wrapper |
| `isc_event_block()` | `fbird_events.c` | Manual EPB construction or `IUtil::getEventBlock()` |
| `isc_event_counts()` | `fbird_events.c` | `IUtil::decodeEvents()` |
| `isc_array_lookup_bounds()` | `fbird_query_array.c` | System table query (RDB$FIELD_DIMENSIONS) |
| `isc_array_get_slice()` | `fbird_query_array.c` | `IAttachment::getSlice()` |
| `isc_array_put_slice()` | `fbird_query_array.c` | `IAttachment::putSlice()` |
| `isc_encode_sql_date()` | `fbird_datetime.c` | Direct struct manipulation |
| `isc_encode_sql_time()` | `fbird_datetime.c` | Direct struct manipulation |
| `isc_encode_timestamp()` | `fbird_datetime.c` | Direct struct manipulation |
| `isc_decode_sql_date()` | `fbird_datetime.c` | Direct struct manipulation |
| `isc_decode_sql_time()` | `fbird_datetime.c` | Direct struct manipulation |
| `isc_decode_timestamp()` | `fbird_datetime.c` | Direct struct manipulation |
| `isc_free()` | `fbird_events.c` | `free()` (isc_free is just a wrapper) |
| `isc_attach_database()` | — | Already replaced by `fbc_connect()` |
| `isc_detach_database()` | — | Already replaced by `fbc_disconnect()` |

### Status-Vector Pointer-Smuggling Hack

In `fbird_connection.c:260-262`, `_php_fbird_attach_db()` stores the OO connection
pointer in the **last slot of the ISC status vector**:

```c
/* Store the OO API connection pointer in the status vector's last slot
 * for retrieval by _php_fbird_connect() */
IBG(status[ISC_STATUS_LENGTH - 1]) = (ISC_STATUS)(uintptr_t)connection;
```

This is retrieved at line 411:
```c
ib_link->fbc_connection = (void *)(uintptr_t)IBG(status[ISC_STATUS_LENGTH - 1]);
IBG(status[ISC_STATUS_LENGTH - 1]) = 0;
```

This hack exists because `_php_fbird_attach_db()` has a legacy signature returning
`void **db` (the old `isc_db_handle`), and the OO pointer had to be smuggled through
a side channel. The fix is to return the `fb::Connection*` directly.

### Compatibility Shim Layer

`src/php_fbird_compat.h` provides dual-mode helpers:

```c
static inline bool fbird_link_is_valid(const fbird_db_link *link) {
    if (link->fbc_connection != NULL) {
        return link->fbc_connection != NULL;
    } else {
        return link->handle.db != 0;
    }
}
```

Once the legacy handles are removed, these shims collapse to simple OO checks.

---

## User Stories / Acceptance Goals

### Goal 1: Single Connection Holder

**Given** a `fbird_db_link` struct
**When** any code accesses the database attachment
**Then** it uses `fbc_connection` exclusively — no `handle.db` field exists

**Success Criteria**:
- [ ] `fb_safe_handle` union removed from `php_fbird_includes.h`
- [ ] Zero `handle.db`, `handle.tr`, `bl_handle.blob` references in any `.c` file
- [ ] `fbird_get_attachment(link)` inline helper used everywhere

### Goal 2: No Legacy isc_*() Function Calls

**Given** the extension source code
**When** compiled against FB 3.0+ client library
**Then** no `isc_*()` function calls remain (only `isc_*` type constants are allowed)

**Success Criteria**:
- [ ] `grep -rn 'isc_[a-z].*(' *.c` returns zero matches (excluding comments/strings)
- [ ] All replaced with OO API equivalents or direct struct manipulation

### Goal 3: Clean Attach/Detach Path

**Given** `_php_fbird_attach_db()` is called
**When** a new connection is created
**Then** the `fb::Connection*` is returned directly — no status-vector smuggling

**Success Criteria**:
- [ ] `_php_fbird_attach_db()` signature changed to return `void*` (connection pointer)
- [ ] No writes to `IBG(status[ISC_STATUS_LENGTH - 1])`

### Goal 4: FB 2.5 Server Compatibility

**Given** the extension is compiled with FB 3.0+ client library
**When** connecting to a Firebird 2.5 server
**Then** all operations work correctly (FB 3.0 client OO API supports FB 2.5 servers)

**Success Criteria**:
- [ ] Documented that FB 3.0+ client is required at compile time
- [ ] No runtime dependency on FB 3.0+ server features

---

## Out of Scope

- Removing `isc_*` **type constants** (e.g., `isc_info_base_level`, `ISC_STATUS`) — these are header-defined values, not function calls
- Statement handle (`fbird_result_set.stmt`) OO migration — deferred to a future "Statement Simplification" spec
- Service API (`fbird_service.c`) handle migration — uses separate `isc_service_*` API with its own OO wrapper already in `fb_service.hpp`
- PHP API changes — all changes are internal; `fbird_*` function signatures unchanged
- Firebird 2.5 **client library** support — FB 3.0+ client is already required

---

## Constitution Alignment

| Article | Requirement | How Met |
|---------|-------------|---------|
| II: Test-First | Tests before implementation | Each phase writes/updates `.phpt` tests before modifying C code |
| III: Memory Safety | ASan/UBSan clean | Sanitizer run after each phase; Valgrind after final phase |
| VI: Atomic Commits | ≤200 LOC per commit | Each task in tasks.md is scoped to ≤200 changed lines |
| VII: Coverage Gate | ≥80% on touched files | Coverage measured after each phase via `coverage.sh` |
| VIII: Docker Testing | Tests in Docker | All tests via `docker compose run --rm php83-dev` |

---

## Dependencies

- **Requires**: None (all OO wrappers already exist in `src/cpp/`)
- **Blocks**: Future statement handle simplification, scrollable cursor support

---

## Open Questions

- [ ] Should `fbird_result_set.stmt` (statement handle) be migrated in this spec or deferred? — Recommended: Defer to separate spec
- [ ] Is `isc_free()` in events just `free()` on all platforms? — Recommended: Yes, replace with `efree()`/`free()` directly
- [ ] Do `isc_encode_*`/`isc_decode_*` datetime functions have OO equivalents? — Recommended: No, use direct `ISC_DATE`/`ISC_TIME`/`ISC_TIMESTAMP` struct manipulation (these are trivial arithmetic)
