# Firebird OO API Reference (3.0+)

## Executive Summary

This document serves as the **authoritative API reference** for the php-firebird extension's Object-Oriented API implementation. The OO API (introduced in Firebird 3.0) is fully implemented and supports all query execution features.

**Status**: All core OO API wrappers complete. Dual-mode architecture operational (OO API alongside legacy for compatibility). See [MODERNIZATION_PLAN_FB3_TO_FB5.md](./MODERNIZATION_PLAN_FB3_TO_FB5.md) for detailed migration progress.

**Key Points**:
- All statement execution types fully supported via OO API
- RAII wrappers provide type safety and automatic resource management
- Firebird 4.0.5 client library compatibility verified
- 94/102 tests passing (remaining 8 are legacy blob path issues pending final legacy removal)

---

## Feature Support Matrix

| Feature | OO API Support | Key Method | Parameters |
|---------|----------------|------------|------------|
| **SELECT queries** | ✅ FULLY SUPPORTED | `IStatement::openCursor()` | `inMetadata`, `inBuffer` for params |
| **DML (INSERT/UPDATE/DELETE)** | ✅ FULLY SUPPORTED | `IStatement::execute()` | `inMetadata`, `inBuffer` for params |
| **DML with RETURNING** | ✅ FULLY SUPPORTED | `IStatement::execute()` | `outMetadata`, `outBuffer` for output |
| **EXECUTE PROCEDURE** | ✅ FULLY SUPPORTED | `IStatement::execute()` | Both `in*` and `out*` params |
| **Parameterized queries** | ✅ FULLY SUPPORTED | Any execute method | `inMetadata`, `inBuffer` |
| **Batch operations** | 🚧 Planned (Not exposed) | `IStatement::createBatch()` | `IBatch` interface |
| **Statement timeout** | 🚧 Planned (Not exposed) | `IStatement::setTimeout()` | milliseconds |
| **Scrollable cursors** | ✅ FULLY SUPPORTED | `IResultSet::fetch*()` | Multiple fetch methods |

---

## Interface Reference

### 1. IStatement Interface

The core interface for prepared statements. **Replaces all `isc_dsql_*` functions.**

**Source**: `/usr/include/firebird/IdlFbInterfaces.h`

#### Key Methods

```cpp
// Execute DML or DDL (with optional input/output parameters)
ITransaction* execute(
    IStatus* status,
    ITransaction* transaction,
    IMessageMetadata* inMetadata,   // Input parameter metadata (or NULL)
    void* inBuffer,                  // Input parameter data buffer (or NULL)
    IMessageMetadata* outMetadata,   // Output metadata for RETURNING (or NULL)
    void* outBuffer                  // Output buffer for RETURNING (or NULL)
);

// Open cursor for SELECT (with optional input parameters)
IResultSet* openCursor(
    IStatus* status,
    ITransaction* transaction,
    IMessageMetadata* inMetadata,   // Input parameter metadata (or NULL)
    void* inBuffer,                  // Input parameter data buffer (or NULL)
    IMessageMetadata* outMetadata,   // Output column metadata (or NULL)
    unsigned flags                   // CURSOR_TYPE_SCROLLABLE or 0
);

// Get metadata about input parameters
IMessageMetadata* getInputMetadata(IStatus* status);

// Get metadata about output columns/fields
IMessageMetadata* getOutputMetadata(IStatus* status);

// Get statement type (SELECT, INSERT, UPDATE, DELETE, DDL, etc.)
unsigned getType(IStatus* status);

// Get number of affected rows after execute
ISC_UINT64 getAffectedRecords(IStatus* status);

// Free statement resources
void free(IStatus* status);
```

#### Statement Type Constants

```cpp
#define isc_info_sql_stmt_select          1
#define isc_info_sql_stmt_insert          2
#define isc_info_sql_stmt_update          3
#define isc_info_sql_stmt_delete          4
#define isc_info_sql_stmt_ddl             5
#define isc_info_sql_stmt_exec_procedure  8
#define isc_info_sql_stmt_select_for_upd  12
#define isc_info_sql_stmt_savepoint       14
```

---

### 2. IAttachment Interface

The connection interface. **Replaces `isc_attach_database()` and related functions.**

#### Key Methods

```cpp
// Prepare a statement
IStatement* prepare(
    IStatus* status,
    ITransaction* tra,
    unsigned stmtLength,
    const char* sqlStmt,
    unsigned dialect,
    unsigned flags      // PREPARE_PREFETCH_* flags
);

// Direct execute without prepare (for simple statements)
ITransaction* execute(
    IStatus* status,
    ITransaction* transaction,
    unsigned stmtLength,
    const char* sqlStmt,
    unsigned dialect,
    IMessageMetadata* inMetadata,
    void* inBuffer,
    IMessageMetadata* outMetadata,
    void* outBuffer
);

// Transaction management
ITransaction* startTransaction(
    IStatus* status,
    unsigned tpbLength,
    const unsigned char* tpb
);

// Blob operations
IBlob* createBlob(IStatus* status, ITransaction* transaction, ISC_QUAD* id, ...);
IBlob* openBlob(IStatus* status, ITransaction* transaction, ISC_QUAD* id, ...);

// Array operations
int getSlice(IStatus* status, ITransaction* transaction, ISC_QUAD* id, ...);
void putSlice(IStatus* status, ITransaction* transaction, ISC_QUAD* id, ...);

// Event handling
IEvents* queEvents(IStatus* status, IEventCallback* callback, unsigned length, const unsigned char* events);

// Connection management
void detach(IStatus* status);
void dropDatabase(IStatus* status);

// FB 4.0+ Timeout support
void setStatementTimeout(IStatus* status, unsigned timeOut);
void setIdleTimeout(IStatus* status, unsigned timeOut);
```

---

### 3. ITransaction Interface

Transaction management. **Replaces `isc_start_transaction()` and related functions.**

#### Key Methods

```cpp
void commit(IStatus* status);
void commitRetaining(IStatus* status);
void rollback(IStatus* status);
void rollbackRetaining(IStatus* status);
void getInfo(IStatus* status, unsigned itemsLength, const unsigned char* items, ...);
```

---

### 4. IResultSet Interface

Cursor for fetching query results. **Replaces `isc_dsql_fetch()`.**

#### Key Methods

```cpp
int fetchNext(IStatus* status, void* message);
int fetchPrior(IStatus* status, void* message);   // Scrollable only
int fetchFirst(IStatus* status, void* message);   // Scrollable only
int fetchLast(IStatus* status, void* message);    // Scrollable only
FB_BOOLEAN isEof(IStatus* status);
IMessageMetadata* getMetadata(IStatus* status);
void close(IStatus* status);
```

#### Fetch Return Values

```cpp
static const int RESULT_OK = 0;        // Row fetched successfully
static const int RESULT_NO_DATA = 1;   // No more rows (EOF)
static const int RESULT_ERROR = -1;    // Error occurred
```

---

### 5. IMessageMetadata Interface

Metadata for parameters and columns. **Replaces SQLDA manipulation.**

#### Key Methods

```cpp
unsigned getCount(IStatus* status);
const char* getField(IStatus* status, unsigned index);
const char* getAlias(IStatus* status, unsigned index);
const char* getRelation(IStatus* status, unsigned index);
unsigned getType(IStatus* status, unsigned index);
unsigned getSubType(IStatus* status, unsigned index);
unsigned getLength(IStatus* status, unsigned index);
int getScale(IStatus* status, unsigned index);
unsigned getOffset(IStatus* status, unsigned index);
unsigned getNullOffset(IStatus* status, unsigned index);
unsigned getMessageLength(IStatus* status);
unsigned getCharSet(IStatus* status, unsigned index);
```

---

### 6. IBlob Interface

Blob handling. **Replaces `isc_create_blob()`, `isc_open_blob()`, etc.**

#### Key Methods

```cpp
int getSegment(IStatus* status, unsigned bufferLength, void* buffer, unsigned* segmentLength);
void putSegment(IStatus* status, unsigned length, const void* buffer);
void close(IStatus* status);
void cancel(IStatus* status);
void getInfo(IStatus* status, unsigned itemsLength, const unsigned char* items, ...);
```

---

### 7. IEvents Interface

Event handling. **Replaces `isc_que_events()` and `isc_cancel_events()`.**

#### Key Methods

```cpp
void cancel(IStatus* status);
```

#### IEventCallback Interface

```cpp
class IEventCallback : public IReferenceCounted {
    void eventCallbackFunction(unsigned length, const unsigned char* events);
};
```

---

## Legacy to OO API Mapping

| Legacy API | OO API |
|------------|--------|
| `isc_attach_database()` | `IProvider::attachDatabase()` |
| `isc_detach_database()` | `IAttachment::detach()` |
| `isc_start_transaction()` | `IAttachment::startTransaction()` |
| `isc_commit_transaction()` | `ITransaction::commit()` |
| `isc_rollback_transaction()` | `ITransaction::rollback()` |
| `isc_dsql_prepare()` | `IAttachment::prepare()` |
| `isc_dsql_execute()` | `IStatement::execute()` |
| `isc_dsql_execute2()` | `IStatement::execute()` (with output) |
| `isc_dsql_fetch()` | `IResultSet::fetchNext()` |
| `isc_dsql_free_statement()` | `IStatement::free()` |
| `isc_create_blob2()` | `IAttachment::createBlob()` |
| `isc_open_blob2()` | `IAttachment::openBlob()` |
| `isc_put_segment()` | `IBlob::putSegment()` |
| `isc_get_segment()` | `IBlob::getSegment()` |
| `isc_close_blob()` | `IBlob::close()` |
| `isc_que_events()` | `IAttachment::queEvents()` |
| `isc_cancel_events()` | `IEvents::cancel()` |
| `isc_service_attach()` | `IProvider::attachServiceManager()` |
| `isc_create_database()` | `IUtil::executeCreateDatabase()` |

---

## Implementation Status

### C++ RAII Wrapper Classes

| Wrapper | File | Status |
|---------|------|--------|
| `ConnectionWrapper` | `src/cpp/fb_connection.hpp` | ✅ Complete |
| `TransactionWrapper` | `src/cpp/fb_transaction.hpp` | ✅ Complete |
| `StatementWrapper` | `src/cpp/fb_statement.hpp` | ✅ Complete |
| `BlobWrapper` | `src/cpp/fb_blob.hpp` | ✅ Complete |
| `EventsWrapper` | `src/cpp/fb_events.hpp` | ✅ Complete |
| `ServiceWrapper` | `src/cpp/fb_service.hpp` | ✅ Complete |
| `ArrayWrapper` | `src/cpp/fb_array.hpp` | ✅ Complete |

### C Interop Function Families

| Family | Count | Functions |
|--------|-------|-----------|
| `fbc_*` (Connection) | 7 | `fbc_connect`, `fbc_disconnect`, `fbc_drop_database`, `fbc_create_database`, `fbc_is_connected`, `fbc_get_attachment`, `fbc_get_server_version` |
| `fbt_*` (Transaction) | 9 | `fbt_start`, `fbt_commit`, `fbt_rollback`, `fbt_commit_retaining`, `fbt_rollback_retaining`, `fbt_get_transaction`, `fbt_is_active`, `fbt_get_handle`, `fbt_free` |
| `fbs_*` (Statement) | 14 | `fbs_prepare`, `fbs_execute`, `fbs_open_cursor`, `fbs_fetch`, `fbs_close_cursor`, `fbs_free`, `fbs_is_cursor_open`, `fbs_get_affected_rows`, `fbs_get_input_metadata`, `fbs_get_output_metadata`, `fbs_get_input_count`, `fbs_get_output_count`, `fbs_get_statement`, `fbs_is_prepared` |
| `fbm_*` (Metadata) | 13 | `fbm_get_message_length`, `fbm_get_count`, `fbm_get_offset`, `fbm_get_null_offset`, `fbm_get_type`, `fbm_get_subtype`, `fbm_get_length`, `fbm_get_scale`, `fbm_get_charset`, `fbm_get_field`, `fbm_get_alias`, `fbm_get_relation`, `fbm_release` |
| `fbb_*` (Blob) | 11 | `fbb_create`, `fbb_open`, `fbb_put_segment`, `fbb_get_segment`, `fbb_close`, `fbb_cancel`, `fbb_get_info`, `fbb_free`, `fbb_get_blob_id`, `fbb_is_open`, `fbb_get_handle` |
| `fbe_*` (Events) | 6 | `fbe_queue`, `fbe_cancel`, `fbe_has_event_fired`, `fbe_reset_event_fired`, `fbe_get_event_data`, `fbe_is_queued`, `fbe_free` |
| `fbsvc_*` (Service) | 6 | `fbsvc_attach`, `fbsvc_detach`, `fbsvc_start`, `fbsvc_query`, `fbsvc_is_attached`, `fbsvc_free` |
| `fba_*` (Array) | 3 | `fba_lookup_bounds`, `fba_get_slice`, `fba_put_slice` |

**Total**: 69 C interop functions

---

## Parameter Binding Pattern

The OO API uses **message buffers** with **metadata descriptors**:

```cpp
// 1. Prepare statement
IStatement* stmt = attachment->prepare(status, transaction, 0, sql, dialect, flags);

// 2. Get input parameter metadata
IMessageMetadata* inMeta = stmt->getInputMetadata(status);

// 3. Allocate and fill input buffer
unsigned char* inBuffer = new unsigned char[inMeta->getMessageLength(status)];
for (unsigned i = 0; i < inMeta->getCount(status); i++) {
    unsigned offset = inMeta->getOffset(status, i);
    unsigned nullOffset = inMeta->getNullOffset(status, i);
    // Set data at offset, null indicator at nullOffset
}

// 4. Execute with parameters
stmt->execute(status, transaction, inMeta, inBuffer, nullptr, nullptr);
```

### For RETURNING Clause / Stored Procedures

```cpp
// Get both input and output metadata
IMessageMetadata* inMeta = stmt->getInputMetadata(status);
IMessageMetadata* outMeta = stmt->getOutputMetadata(status);

// Allocate both buffers
unsigned char* inBuffer = new unsigned char[inMeta->getMessageLength(status)];
unsigned char* outBuffer = new unsigned char[outMeta->getMessageLength(status)];

// Execute with both
stmt->execute(status, transaction, inMeta, inBuffer, outMeta, outBuffer);
```

---

## Version Compatibility

| Feature | FB 3.0 | FB 4.0 | FB 5.0 |
|---------|--------|--------|--------|
| Core OO API | ✅ | ✅ | ✅ |
| `IStatement::setTimeout()` | ❌ | ✅ | ✅ |
| `IBatch` interface | ❌ | ✅ | ✅ |
| `IAttachment::setIdleTimeout()` | ❌ | ✅ | ✅ |
| `Scrollable cursors` | ✅ | ✅ | ✅ |
| `IStatus::hasData()` | ❌ | ❌ | ✅ |

**Note**: For FB 4.0 compatibility, use `statusHasError()` helper instead of `IStatus::hasData()`.
**Note**: Some FB 4.0+ features (timeout, batch) are available in the underlying API but may not yet be exposed via the extension's C interface.

---

## Related Documentation

- **[MODERNIZATION_PLAN_FB3_TO_FB5.md](./MODERNIZATION_PLAN_FB3_TO_FB5.md)** - Detailed migration progress, phase notes, commit history
- **[FBIRD_QUERY_EXEC_FIXES.md](./FBIRD_QUERY_EXEC_FIXES.md)** - Query execution fixes and parameter passing details

---

## Document History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-12-13 | Initial reference created from IdlFbInterfaces.h inspection |
| 1.1 | 2025-12-13 | Updated status, consolidated with modernization plan, added cross-references |
| 1.2 | 2025-12-13 | Updated completeness check: added missing C interop functions (metadata, etc.) and clarified supported features status |
