# Firebird Client C API Research — v3.0, v4.0, v5.0

> Date: 2026-03-22
> Purpose: Document all public OO API interfaces, deprecated isc_* functions still in use,
> known client library issues/caveats, and wire protocol feature matrix.

---

## 1. OO API Interface Inventory

### 1.1 IAttachment (Connection)

Available since FB 3.0 client (`FB_API_VER >= 30`):

| Method | Description |
|--------|-------------|
| `getInfo()` | Database information (replaces `isc_database_info()`) |
| `startTransaction()` | Start transaction with TPB (replaces `isc_start_transaction()`) |
| `reconnectTransaction()` | Reconnect to limbo transaction |
| `compileRequest()` | Compile BLR request |
| `transactRequest()` | Execute BLR request |
| `createBlob()` | Create new blob (replaces `isc_create_blob2()`) |
| `openBlob()` | Open existing blob (replaces `isc_open_blob2()`) |
| `getSlice()` | Read array slice (replaces `isc_array_get_slice()`) |
| `putSlice()` | Write array slice (replaces `isc_array_put_slice()`) |
| `executeDyn()` | Execute dynamic SQL |
| `prepare()` | Prepare statement (replaces `isc_dsql_prepare()`) |
| `execute()` | Execute statement without cursor |
| `openCursor()` | Execute statement returning cursor/IResultSet |
| `queEvents()` | Queue async event notification (replaces `isc_que_events()`) |
| `cancelOperation()` | Cancel running operation |
| `ping()` | Check connection liveness |
| `deprecatedDetach()` | Detach (FB3 compat, throws on error) |
| `deprecatedDropDatabase()` | Drop database (FB3 compat) |

Added in FB 4.0 (`FB_API_VER >= 40`):

| Method | Description |
|--------|-------------|
| `getIdleTimeout()` | Get idle timeout (seconds) |
| `setIdleTimeout()` | Set idle timeout |
| `getStatementTimeout()` | Get statement timeout |
| `setStatementTimeout()` | Set statement timeout |
| `createBatch()` | Create batch DML object |
| `createReplicator()` | Create replication object |
| `detach()` | Detach (replaces `deprecatedDetach()`, returns void) |
| `dropDatabase()` | Drop database (replaces `deprecatedDropDatabase()`) |

Added in FB 5.0 (`FB_API_VER >= 50`):

| Method | Description |
|--------|-------------|
| `getMaxBlobCacheSize()` | Get max blob cache size |
| `setMaxBlobCacheSize()` | Set max blob cache size |
| `getMaxInlineBlobSize()` | Get max inline blob size |
| `setMaxInlineBlobSize()` | Set max inline blob size |

### 1.2 ITransaction

Available since FB 3.0:

| Method | Description |
|--------|-------------|
| `getInfo()` | Transaction info (replaces `isc_transaction_info()`) |
| `prepare()` | Two-phase prepare |
| `deprecatedCommit()` | Commit (FB3 compat) |
| `commitRetaining()` | Commit retaining |
| `deprecatedRollback()` | Rollback (FB3 compat) |
| `rollbackRetaining()` | Rollback retaining |
| `deprecatedDisconnect()` | Disconnect (FB3 compat) |
| `commit()` | Commit (FB4+, replaces deprecated) |
| `rollback()` | Rollback (FB4+, replaces deprecated) |
| `disconnect()` | Disconnect (FB4+, replaces deprecated) |

### 1.3 IStatement

Available since FB 3.0:

| Method | Description |
|--------|-------------|
| `getInfo()` | Statement info |
| `getType()` | Statement type (SELECT, INSERT, etc.) |
| `getPlan()` | Execution plan text |
| `getAffectedRecords()` | Rows affected |
| `getInputMetadata()` | Input parameter metadata |
| `getOutputMetadata()` | Output column metadata |
| `execute()` | Execute (non-cursor) |
| `openCursor()` | Execute returning IResultSet |
| `setCursorName()` | Set cursor name for positioned updates |
| `deprecatedFree()` | Free statement (FB3 compat) |
| `getFlags()` | Statement flags |

Added in FB 4.0:

| Method | Description |
|--------|-------------|
| `getTimeout()` | Get statement timeout |
| `setTimeout()` | Set statement timeout |
| `createBatch()` | Create batch from prepared statement |
| `free()` | Free statement (replaces `deprecatedFree()`) |

Added in FB 5.0:

| Method | Description |
|--------|-------------|
| `getMaxInlineBlobSize()` | Get max inline blob size |
| `setMaxInlineBlobSize()` | Set max inline blob size |

### 1.4 IResultSet

Available since FB 3.0:

| Method | Description |
|--------|-------------|
| `fetchNext()` | Fetch next row |
| `fetchPrior()` | Fetch previous row (scrollable) |
| `fetchFirst()` | Fetch first row (scrollable) |
| `fetchLast()` | Fetch last row (scrollable) |
| `fetchAbsolute()` | Fetch by absolute position (scrollable) |
| `fetchRelative()` | Fetch by relative offset (scrollable) |
| `isEof()` | Check end of result set |
| `isBof()` | Check beginning of result set |
| `getMetadata()` | Get output metadata |
| `deprecatedClose()` | Close (FB3 compat) |
| `setDelayedOutputFormat()` | Set delayed output format |

Added in FB 4.0+:

| Method | Description |
|--------|-------------|
| `close()` | Close (replaces `deprecatedClose()`) |
| `getInfo()` | Result set info |

### 1.5 IBlob

Available since FB 3.0:

| Method | Description |
|--------|-------------|
| `getInfo()` | Blob info (replaces `isc_blob_info()`) |
| `getSegment()` | Read segment (replaces `isc_get_segment()`) |
| `putSegment()` | Write segment (replaces `isc_put_segment()`) |
| `deprecatedCancel()` | Cancel (FB3 compat) |
| `deprecatedClose()` | Close (FB3 compat) |
| `seek()` | Seek in stream blob |
| `cancel()` | Cancel (FB4+) |
| `close()` | Close (FB4+) |

### 1.6 IBatch (FB 4.0+)

| Method | Description |
|--------|-------------|
| `add()` | Add parameter set |
| `addBlob()` | Add blob to batch |
| `appendBlobData()` | Append data to batch blob |
| `addBlobStream()` | Add blob via stream |
| `registerBlob()` | Register existing blob |
| `execute()` | Execute batch |
| `cancel()` | Cancel batch |
| `getBlobAlignment()` | Get blob alignment |
| `getMetadata()` | Get batch metadata |
| `setDefaultBpb()` | Set default blob parameters |
| `deprecatedClose()` | Close (FB4 compat) |
| `close()` | Close (FB4.0.1+) |
| `getInfo()` | Batch info |

### 1.7 IService

Available since FB 3.0:

| Method | Description |
|--------|-------------|
| `deprecatedDetach()` | Detach (FB3 compat) |
| `query()` | Query service (replaces `isc_service_query()`) |
| `start()` | Start service task (replaces `isc_service_start()`) |
| `detach()` | Detach (FB4+) |
| `cancel()` | Cancel running service task (FB5+) |

### 1.8 IEvents

Available since FB 3.0:

| Method | Description |
|--------|-------------|
| `deprecatedCancel()` | Cancel (FB3 compat) |
| `cancel()` | Cancel (FB4+) |

### 1.9 IMessageMetadata

Available since FB 3.0:

| Method | Description |
|--------|-------------|
| `getCount()` | Number of fields |
| `getField()` | Field name |
| `getRelation()` | Table name |
| `getOwner()` | Owner name |
| `getAlias()` | Column alias |
| `getType()` | SQL type |
| `isNullable()` | Nullable flag |
| `getSubType()` | Blob/text subtype |
| `getLength()` | Field length |
| `getScale()` | Numeric scale |
| `getCharSet()` | Character set ID |
| `getOffset()` | Offset in message buffer |
| `getNullOffset()` | Null indicator offset |
| `getBuilder()` | Get metadata builder |
| `getMessageLength()` | Total message buffer size |

Added in FB 4.0:

| Method | Description |
|--------|-------------|
| `getAlignment()` | Field alignment |
| `getAlignedLength()` | Aligned field length |

---

## 2. Deprecated isc_* Functions Still in Use

### 2.1 Active isc_* Function Calls in Extension Code

| Function | File(s) | Count | OO API Replacement |
|----------|---------|-------|--------------------|
| `isc_vax_integer()` | fbird_blobs.c, fbird_result.c, fbird_service.c, fbird_transaction.c | ~25 | **Utility function** — no OO replacement needed, safe to keep or inline |
| `isc_service_attach()` | fbird_service.c | 1 | `IProvider::attachServiceManager()` |
| `isc_service_detach()` | fbird_service.c | 1 | `IService::detach()` |
| `isc_service_start()` | fbird_service.c | 3 | `IService::start()` |
| `isc_service_query()` | fbird_service.c | 1 | `IService::query()` |
| `isc_sqlcode()` | fbird_error.c | 1 | `IStatus::getErrors()` or `IUtil::formatStatus()` |
| `isc_encode_timestamp()` | fbird_query_array.c | 1 | `IUtil::encodeTimestamp()` (FB4+) or keep |
| `isc_encode_sql_date()` | fbird_query_array.c | 1 | `IUtil::encodeDate()` (FB4+) or keep |
| `isc_encode_sql_time()` | fbird_query_array.c | 1 | `IUtil::encodeTime()` (FB4+) or keep |
| `isc_wait_for_event()` | fb_events.hpp | 1 | `IAttachment::queEvents()` + `IEvents` async |

### 2.2 isc_* Constants Still in Use (KEEP — these are protocol constants)

- `isc_tpb_*` — Transaction Parameter Block constants
- `isc_dpb_*` — Database Parameter Block constants
- `isc_spb_*` — Service Parameter Block constants
- `isc_info_*` — Info request item constants
- `isc_bpb_*` — Blob Parameter Block constants
- `isc_action_*` — Service action constants
- `isc_sdl_*` — SDL (array descriptor) constants
- `isc_bad_segstr_handle` — Error code constant
- `isc_segment` — Status constant for partial segment read

These are **not deprecated** — they are wire protocol constants defined in `ibase.h`.

### 2.3 Legacy Handle Types Still in Use

| Type | Location | Usage |
|------|----------|-------|
| `fb_safe_handle` (union) | `php_fbird_includes.h` | Only for `_ib_query.stmt` (isc_stmt_handle) |
| `isc_svc_handle` | `fbird_service.c` | Service manager handle (5 calls) |
| `ISC_STATUS_ARRAY` | Multiple files | Status vector for error reporting |

---

## 3. Known Client Library Issues and Caveats

### 3.1 Wire Protocol Compatibility

The Firebird client library is **forward-compatible**: a FB 5.0 client can connect to
FB 2.5, 3.0, 4.0, and 5.0 servers. The OO API (`IAttachment`, etc.) works with all
server versions — the client negotiates the wire protocol version automatically.

**Caveat**: Features requiring server support (e.g., DECFLOAT, INT128, time zones,
batch DML) will fail with appropriate errors when used against older servers.

### 3.2 Wire Protocol Version Matrix

| Client \ Server | FB 2.5 | FB 3.0 | FB 4.0 | FB 5.0 |
|-----------------|--------|--------|--------|--------|
| FB 3.0 client | Wire 12 | Wire 13 | N/A | N/A |
| FB 4.0 client | Wire 12 | Wire 13 | Wire 16 | N/A |
| FB 5.0 client | Wire 12 | Wire 13 | Wire 16 | Wire 18 |

Wire protocol features:
- **Wire 12** (FB 2.5): Basic SQL, blobs, arrays, events
- **Wire 13** (FB 3.0): SRP authentication, wire encryption, boolean type
- **Wire 16** (FB 4.0): Batch DML, time zones, DECFLOAT, INT128, statement timeouts
- **Wire 18** (FB 5.0): Inline blobs, parallel operations, profiler

### 3.3 Known Issues

1. **Windows XNET local protocol**: Requires exact version match between client and
   server DLLs. TCP/IP (`inet://`) works across versions.

2. **`deprecatedDetach()` vs `detach()`**: FB3 client only has `deprecatedDetach()`
   which throws on error. FB4+ `detach()` returns void and is silent on error.
   Extension must use `#if FB_API_VER >= 40` guards.

3. **`isc_wait_for_event()` blocking**: This legacy function blocks the calling thread
   indefinitely. The OO API `IAttachment::queEvents()` + `IEvents` provides async
   notification but requires a callback mechanism. Current extension uses SIGALRM
   timeout workaround.

4. **`isc_vax_integer()` is NOT deprecated**: It's a utility function for decoding
   little-endian integers from info buffers. No OO replacement exists or is needed.
   Can be replaced with inline `le32toh()`/`le16toh()` if desired.

5. **`isc_sqlcode()` deprecation**: FB4+ recommends using `IStatus::getErrors()` to
   extract SQLSTATE instead of legacy SQLCODE. However, `isc_sqlcode()` still works
   and many PHP applications depend on the numeric SQL code.

6. **Service API**: The OO API provides `IService` but the SPB/info buffer format
   is identical to the legacy API. Migration is straightforward but low-value since
   the buffer parsing code (`isc_vax_integer()` calls) remains the same.

7. **Array API**: `isc_array_lookup_bounds()` has been replaced by querying
   `RDB$FIELD_DIMENSIONS` system table directly (already done in `fb_array.hpp`).
   `getSlice()`/`putSlice()` on `IAttachment` replace `isc_array_get/put_slice()`.

8. **Statement handle lifecycle**: The legacy `isc_stmt_handle` is still needed for
   `isc_dsql_*` functions. The OO API `IStatement` manages its own lifecycle via
   `free()` (FB4+) or `deprecatedFree()` (FB3). Current extension uses `fb_safe_handle`
   union for `_ib_query.stmt` — this is the last remaining legacy handle.

9. **Thread safety**: The OO API is thread-safe by design (each interface has its own
   reference counting). The legacy API requires external synchronization. PHP's ZTS
   mode benefits from the OO API's built-in thread safety.

10. **Memory management**: OO API objects must be explicitly released (`release()` or
    `dispose()`). Failure to do so leaks memory. The extension's C++ wrappers handle
    this via RAII destructors.

---

## 4. Remaining isc_* Elimination Roadmap

### Priority 1: Service API (fbird_service.c) — 5 calls
- Replace `isc_service_attach()` → `IProvider::attachServiceManager()`
- Replace `isc_service_detach()` → `IService::detach()`
- Replace `isc_service_start()` → `IService::start()`
- Replace `isc_service_query()` → `IService::query()`
- Already have `fb_service.hpp` C++ wrapper — needs extern C bridge functions

### Priority 2: Statement handle (_ib_query.stmt) — last fb_safe_handle
- Replace `isc_dsql_*` calls with `IStatement` methods
- Remove `fb_safe_handle` union entirely from `php_fbird_includes.h`
- Largest change — touches query prepare, execute, bind, result

### Priority 3: Events (fbird_events.c) — 1 call
- Replace `isc_wait_for_event()` with `IAttachment::queEvents()` + async callback
- Requires redesign of polling model

### Priority 4: Utility functions — safe to keep
- `isc_vax_integer()` — utility, no replacement needed
- `isc_sqlcode()` — can migrate to `IStatus` but low priority
- `isc_encode_*` — can migrate to `IUtil` but low priority

---

## 5. Feature Matrix for PHP Extension

| Feature | FB 2.5 Server | FB 3.0 Server | FB 4.0 Server | FB 5.0 Server |
|---------|---------------|---------------|---------------|---------------|
| Basic SQL (SELECT/INSERT/UPDATE/DELETE) | ✅ | ✅ | ✅ | ✅ |
| Prepared statements | ✅ | ✅ | ✅ | ✅ |
| Transactions (commit/rollback/retaining) | ✅ | ✅ | ✅ | ✅ |
| Blobs (stream/segment) | ✅ | ✅ | ✅ | ✅ |
| Arrays | ✅ | ✅ | ✅ | ✅ |
| Events (sync polling) | ✅ | ✅ | ✅ | ✅ |
| Service API (backup/restore/users) | ✅ | ✅ | ✅ | ✅ |
| Persistent connections | ✅ | ✅ | ✅ | ✅ |
| BOOLEAN type | ❌ | ✅ | ✅ | ✅ |
| Wire encryption (SRP) | ❌ | ✅ | ✅ | ✅ |
| Batch DML | ❌ | ❌ | ✅ | ✅ |
| DECFLOAT type | ❌ | ❌ | ✅ | ✅ |
| INT128 type | ❌ | ❌ | ✅ | ✅ |
| Time zones (TIMESTAMP WITH TZ) | ❌ | ❌ | ✅ | ✅ |
| Statement timeouts | ❌ | ❌ | ✅ | ✅ |
| Idle timeouts | ❌ | ❌ | ✅ | ✅ |
| Scrollable cursors | ✅ | ✅ | ✅ | ✅ |
| Named cursors | ✅ | ✅ | ✅ | ✅ |
| Inline blobs | ❌ | ❌ | ❌ | ✅ |
| Parallel operations | ❌ | ❌ | ❌ | ✅ |
| Profiler API | ❌ | ❌ | ❌ | ✅ |

---

## 6. Pre-existing Test Failures Analysis

### 6.1 `tests/issue23_alias_padding_001.phpt` (PHP 8.2 / FB 4.0 CI)

**Status**: Passes locally in all tested containers (php82-dev, php83-dev).
The SKIPIF section restricts to FB 4.0 only (`skip_if_fb_lt(4.0); skip_if_fb_gte(5.0)`).
CI failure was **transient** — likely a timing/container startup issue.
The test itself is correct and the alias deduplication logic works.

### 6.2 Split Stubs Package CI

**Status**: Fails because tag `v7.3.6-rc1` already exists in the target
`satwareAG/php-firebird-stubs` repository from a previous split.
**Fix**: The split-stubs workflow should use `git tag -f` (force) or skip
if tag exists. This is a **workflow configuration issue**, not a code bug.

---

## 7. Recommendations for Next-Gen Architecture

1. **Complete isc_* elimination** in priority order (service → stmt → events)
2. **Layer 2 OOP classes** should map 1:1 to OO API interfaces for maintainability
3. **Layer 3 PDO driver** must use `fbird:` DSN prefix to avoid collision with
   bundled `pdo_firebird` (which uses `firebird:` prefix)
4. **FB version detection at runtime** via `IAttachment::getInfo()` to enable/disable
   features dynamically rather than compile-time `#if FB_API_VER` guards
5. **Async events** should be deferred to a future phase — current sync polling works
