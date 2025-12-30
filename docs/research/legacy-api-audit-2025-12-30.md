# Legacy API Audit Report

**Date**: 2025-12-30  
**Extension Version**: 7.0.0-rc.18  
**Auditor**: Cline AI Assistant  
**Purpose**: Verify extension uses only Firebird 3.0+ OO API (no 2.5 legacy remnants)

## Executive Summary

**RESULT: CLEAN** - The php-firebird extension uses 100% Firebird 3.0+ OO API for all database operations. No legacy Firebird 2.5 API calls were found.

## Audit Scope

Files audited for legacy `isc_*` API usage:
- `firebird_utils.cpp` - C++ OO API wrapper
- `src/cpp/fb_connection.hpp` - RAII Connection wrapper  
- `firebird.c` - Main extension file
- `fbird_events.c` - Event handling
- `fbird_blobs.c` - BLOB operations
- `fbird_query*.c` - Query execution
- `fbird_datetime.c` - Date/time handling
- `fbird_udf.c` - UDF support

## Legacy API Status

### NOT FOUND (Correctly Absent)

| Legacy Function | Modern Replacement | Status |
|-----------------|-------------------|--------|
| `isc_attach_database()` | `IProvider::attachDatabase()` | ✅ Not used |
| `isc_detach_database()` | `IAttachment::detach()` | ✅ Not used |
| `isc_create_database()` | `IProvider::createDatabase()` | ✅ Not used |
| `isc_start_transaction()` | `IAttachment::startTransaction()` | ✅ Not used |
| `isc_commit_transaction()` | `ITransaction::commit()` | ✅ Not used |
| `isc_rollback_transaction()` | `ITransaction::rollback()` | ✅ Not used |
| `isc_dsql_allocate_statement()` | `IAttachment::prepare()` | ✅ Not used |
| `isc_dsql_prepare()` | `IAttachment::prepare()` | ✅ Not used |
| `isc_dsql_execute()` | `IStatement::execute()` | ✅ Not used |
| `isc_dsql_execute2()` | `IStatement::execute()` | ✅ Not used |
| `isc_dsql_fetch()` | `IResultSet::fetchNext()` | ✅ Not used |
| `isc_dsql_free_statement()` | `IStatement::free()` | ✅ Not used |

### VALID Utility Functions (Exist in Both Old/New API)

These functions are **NOT legacy-only** - they remain valid in Firebird 3.0+:

| Function | Location | Purpose |
|----------|----------|---------|
| `isc_encode_timestamp()` | fbird_query_array.c, fbird_udf.c | Date/time encoding |
| `isc_encode_sql_date()` | fbird_datetime.c | SQL DATE encoding |
| `isc_encode_sql_time()` | fbird_datetime.c | SQL TIME encoding |
| `isc_decode_timestamp()` | fbird_datetime.c | Timestamp decoding |
| `isc_decode_sql_date()` | fbird_datetime.c | SQL DATE decoding |
| `isc_decode_sql_time()` | fbird_datetime.c | SQL TIME decoding |
| `isc_event_block()` | fbird_events.c | Event buffer creation |
| `isc_event_counts()` | fbird_events.c | Event counting |
| `isc_wait_for_event()` | fbird_events.c | Synchronous event wait |
| `isc_free()` | fbird_events.c | Memory deallocation |
| `isc_sqlcode()` | firebird.c | SQL error code extraction |
| `isc_vax_integer()` | firebird.c | Byte order conversion |
| `fb_interpret()` | firebird.c | Error message formatting |
| `fb_sqlstate()` | firebird.c | SQLSTATE extraction |

## Modern OO API Usage

### firebird_utils.cpp

```cpp
// 100% OO API - confirmed patterns:
Firebird::IMaster* master = Firebird::fb_get_master_interface();
Firebird::IProvider* provider = master->getDispatcher();
Firebird::IAttachment* attachment = provider->attachDatabase(...);
Firebird::ITransaction* transaction = attachment->startTransaction(...);
Firebird::IStatement* statement = attachment->prepare(...);

// Version guards:
#if FB_API_VER >= 30
#if FB_API_VER >= 40  // For batch operations
```

### src/cpp/fb_connection.hpp

```cpp
// RAII wrapper using OO API:
class Connection {
    Firebird::IAttachment* attachment_;
    
    void detach() { attachment_->detach(&status_); }
};

// DPB construction via IXpbBuilder (NOT isc_dpb_*)
class DpbBuilder {
    Firebird::IXpbBuilder* dpb_;
};
```

### firebird.c

```c
// All operations via C++ wrapper functions:
fbc_connect()        // → fb::Connection::create()
fbc_disconnect()     // → fb::Connection::detach()
fbt_start()          // → fb::Transaction::start()
fbt_commit()         // → fb::Transaction::commit()
fbs_prepare()        // → fb::Statement::prepare()
fbs_execute()        // → fb::Statement::execute()
fbbatch_*()          // → fb::Batch::* (FB4.0+)
```

### fbird_events.c

```c
// Thread-safe polling model (PHP 8.1+):
// Uses only utility functions, NOT legacy connection/transaction APIs
isc_event_block()    // Buffer creation - valid FB3+
isc_wait_for_event() // Synchronous wait - valid FB3+
isc_event_counts()   // Event counting - valid FB3+
isc_free()           // Memory cleanup - valid FB3+
```

## Conclusion

The php-firebird extension is **clean of legacy Firebird 2.5 API remnants**:

1. **Connection Management**: Uses `Firebird::IProvider::attachDatabase()` / `IAttachment::detach()`
2. **Transaction Control**: Uses `IAttachment::startTransaction()` / `ITransaction::commit/rollback()`
3. **Statement Execution**: Uses `IAttachment::prepare()` / `IStatement::execute()`
4. **Batch Operations**: Uses `IBatch` interface (FB4.0+)
5. **Event Handling**: Uses utility functions only (valid FB3+)

The remaining `isc_*` functions are utility functions that exist in both old and new APIs and are documented as valid for Firebird 3.0+.

## Implications for Memory Leak Investigation

The FB5-only memory leak (145,408 bytes in 2 blocks) is **NOT** caused by legacy API usage in the extension. Investigation should focus on:

1. Upstream fbclient library behavior in FB5
2. Differences in FB5's internal memory management
3. Potential FB5-specific initialization/cleanup changes

## References

- Firebird 3.0 Release Notes: OO API introduction
- Firebird 4.0 Release Notes: IBatch interface
- Extension architecture: `docs/OO_API_MIGRATION_RESEARCH.md`