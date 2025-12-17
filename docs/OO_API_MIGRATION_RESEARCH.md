# Firebird C++ OO API Migration Research

Research conducted: 2025-12-17  
Source: DeepWiki analysis of FirebirdSQL/firebird repository

## Executive Summary

The modern Firebird C++ OO API provides structured alternatives for some legacy `isc_*` functions, but **the underlying constants remain the same**. The key modernization is in the interfaces used to work with these constants, not in replacing the constants themselves.

| Category | Legacy API | Modern OO API | Constants |
|----------|------------|---------------|-----------|
| Date/Time | `isc_encode_*`, `isc_decode_*` | `IUtil` methods | N/A |
| Info Requests | `isc_database_info()`, etc. | `IAttachment::getInfo()`, etc. | Same `isc_info_*` |
| Transactions | `isc_start_transaction()` | `IAttachment::startTransaction()` | Same `isc_tpb_*` |
| Services | `isc_service_attach()` | `IProvider::attachServiceManager()` | Same `isc_spb_*` |
| Byte Parsing | `isc_vax_integer()` | `ClumpletReader` | N/A |
| Block Building | Manual byte arrays | `IXpbBuilder` | Same constants |

## Detailed Findings

### 1. Date/Time Functions - ✅ MODERN REPLACEMENTS EXIST

The `IUtil` interface (obtained via `IMaster::getUtilInterface()`) provides modern replacements:

| Legacy Function | Modern Replacement | Notes |
|-----------------|-------------------|-------|
| `isc_decode_sql_date()` | `IUtil::decodeDate()` | Internally calls legacy function |
| `isc_encode_sql_date()` | `IUtil::encodeDate()` | Internally calls legacy function |
| `isc_decode_sql_time()` | `IUtil::decodeTime()` | Internally calls legacy function |
| `isc_encode_sql_time()` | `IUtil::encodeTime()` | Internally calls legacy function |
| `isc_decode_timestamp()` | `IUtil::decodeDate()` + `IUtil::decodeTime()` | Combined for ISC_TIMESTAMP |
| `isc_encode_timestamp()` | `IUtil::encodeDate()` + `IUtil::encodeTime()` | Combined for ISC_TIMESTAMP |

**New Time Zone Methods (Firebird 4.0+):**
- `IUtil::decodeTimeTz()` - Decode `ISC_TIME_TZ`
- `IUtil::encodeTimeTz()` - Encode `ISC_TIME_TZ`
- `IUtil::decodeTimeStampTz()` - Decode `ISC_TIMESTAMP_TZ`
- `IUtil::encodeTimeStampTz()` - Encode `ISC_TIMESTAMP_TZ`
- `IUtil::decodeTimeTzEx()` - Extended time zone decode
- `IUtil::decodeTimeStampTzEx()` - Extended timestamp time zone decode

**Migration Recommendation:** Optional migration. The OO API methods internally call the legacy functions, so no functional difference. Migration provides cleaner API usage and access to timezone features.

### 2. Info Constants (`isc_info_*`) - ⚠️ NO REPLACEMENT, CONSTANTS STILL USED

The `isc_info_*` constants are **still used** with the modern OO API `getInfo()` methods:

| Legacy API | Modern OO API |
|------------|---------------|
| `isc_database_info()` | `IAttachment::getInfo()` |
| `isc_transaction_info()` | `ITransaction::getInfo()` |
| `isc_dsql_sql_info()` | `IStatement::getInfo()` |
| `isc_blob_info()` | `IBlob::getInfo()` |

**The same constants are used:**
- Database info: `isc_info_db_id`, `isc_info_reads`, `isc_info_writes`, etc.
- Statement info: `isc_info_sql_stmt_type`, `isc_info_sql_records`, etc.
- Transaction info: `isc_info_tra_id`, `isc_info_tra_oldest_active`, etc.

**New constants added (prefixed `fb_info_*`):**
- `fb_info_firebird_version`
- `fb_info_implementation`
- `fb_info_ses_idle_timeout_db`
- `fb_info_statement_timeout_db`
- `fb_info_tra_snapshot_number`

**Migration Recommendation:** No migration needed. Use the same constants with OO API methods.

### 3. Transaction Parameter Block (`isc_tpb_*`) - ✅ IXpbBuilder WRAPPER IMPLEMENTED

The `isc_tpb_*` constants are **still used** when building TPBs for `IAttachment::startTransaction()`:

```cpp
// Modern approach using IXpbBuilder
IXpbBuilder* tpb = util->getXpbBuilder(status, IXpbBuilder::TPB, nullptr, 0);
tpb->insertTag(status, isc_tpb_read_committed);  // Same constant!
tpb->insertTag(status, isc_tpb_write);           // Same constant!
tpb->insertTag(status, isc_tpb_nowait);          // Same constant!

// Pass to startTransaction
attachment->startTransaction(status, tpb->getBufferLength(status), tpb->getBuffer(status));
```

**All standard TPB constants remain:**
- `isc_tpb_version3`
- `isc_tpb_write`, `isc_tpb_read`
- `isc_tpb_consistency`, `isc_tpb_concurrency`, `isc_tpb_read_committed`
- `isc_tpb_wait`, `isc_tpb_nowait`
- `isc_tpb_lock_timeout`
- etc.

**Migration Status:** ✅ **IMPLEMENTED** (2025-12-17)

The `fbu_build_tpb()` wrapper in `firebird_utils.cpp` now uses `IXpbBuilder` for type-safe TPB construction:

```cpp
// New OO API wrapper (implemented in firebird_utils.cpp)
fbu_tpb_result_t fbu_build_tpb(
    Firebird::IMaster* master,
    zend_long flags,                    // PHP_FBIRD_* flags
    fbu_table_reservation_t* reserves,  // Table reservations (optional)
    size_t reserve_count,
    zend_long lock_timeout              // Lock timeout in seconds (optional)
);
```

**Features:**
- Type-safe flag-to-TPB-tag mapping
- Support for all isolation levels and access modes
- Table reservation support with lock modes
- Lock timeout support
- Debug logging via `flag_to_name()` helper a

### 4. Service Parameter Block (`isc_spb_*`) - ⚠️ NO REPLACEMENT, CONSTANTS STILL USED

The `isc_spb_*` constants are **still used** with `IProvider::attachServiceManager()` and `IService` methods:

```cpp
// Modern approach using ClumpletWriter or IXpbBuilder
IXpbBuilder* spb = util->getXpbBuilder(status, IXpbBuilder::SPB_ATTACH, nullptr, 0);
spb->insertString(status, isc_spb_user_name, "SYSDBA");  // Same constant!
spb->insertString(status, isc_spb_password, "password"); // Same constant!

// Attach to service manager
IService* svc = provider->attachServiceManager(status, "service_mgr", 
    spb->getBufferLength(status), spb->getBuffer(status));
```

**All standard SPB constants remain:**
- `isc_spb_version`, `isc_spb_current_version`
- `isc_spb_user_name`, `isc_spb_password`
- `isc_spb_command_line`, `isc_spb_dbname`
- `isc_spb_verbose`
- etc.

**Migration Recommendation:** No migration needed for constants. Optional: Use `IXpbBuilder` for cleaner SPB construction.

### 5. Byte Order Conversion (`isc_vax_integer`) - ⚠️ NO DIRECT REPLACEMENT

The `isc_vax_integer()` and `isc_portable_integer()` functions have no direct OO API replacement.

**Modern alternatives for parsing info buffers:**

1. **ClumpletReader** - For parsing structured parameter blocks:
   ```cpp
   Firebird::ClumpletReader reader(buffer, bufferLength);
   while (!reader.isEof()) {
       int tag = reader.getClumpTag();
       int value = reader.getInt();  // Automatic byte order conversion
       reader.moveNext();
   }
   ```

2. **IXpbBuilder** (reading mode) - When parsing response blocks:
   ```cpp
   IXpbBuilder* builder = util->getXpbBuilder(status, IXpbBuilder::INFO_RESPONSE, buf, len);
   while (!builder->isEof(status)) {
       int tag = builder->getTag(status);
       int value = builder->getInt(status);  // Automatic conversion
       builder->moveNext(status);
   }
   ```

**Internal usage remains:**
- Firebird's own utilities (gbak, isql, etc.) still use `isc_vax_integer()` internally
- The functions are still part of the public API

**Migration Recommendation:** For structured blocks, use `ClumpletReader` or `IXpbBuilder`. For simple integer conversions from raw buffers, `isc_vax_integer()` remains valid.

### 6. IXpbBuilder Interface - ✅ NEW STRUCTURED BUILDER

The `IXpbBuilder` interface provides a structured way to build parameter blocks:

**Obtaining IXpbBuilder:**
```cpp
IUtil* util = master->getUtilInterface();
IXpbBuilder* builder = util->getXpbBuilder(status, kind, nullptr, 0);
```

**Builder kinds (`IXpbBuilder::*`):**
| Constant | Purpose |
|----------|---------|
| `DPB` | Database Parameter Block |
| `TPB` | Transaction Parameter Block |
| `SPB_ATTACH` | Service attach parameters |
| `SPB_START` | Service start parameters |
| `SPB_SEND`, `SPB_RECEIVE`, `SPB_RESPONSE` | Service query operations |
| `BPB` | BLOB Parameter Block |
| `BATCH` | Batch operation parameters |
| `INFO_SEND`, `INFO_RESPONSE` | Information request/response |

**Builder methods:**
- `insertInt(status, tag, value)` - Insert integer
- `insertBigInt(status, tag, value)` - Insert 64-bit integer
- `insertBytes(status, tag, bytes, length)` - Insert bytes
- `insertString(status, tag, str)` - Insert string
- `insertTag(status, tag)` - Insert tag without value
- `getBuffer(status)` - Get built buffer
- `getBufferLength(status)` - Get buffer length

**Reader methods (for parsing):**
- `isEof(status)`, `moveNext(status)`, `rewind(status)`
- `findFirst(status, tag)`, `findNext(status, tag)`
- `getTag(status)`, `getLength(status)`
- `getInt(status)`, `getBigInt(status)`, `getString(status)`, `getBytes(status)`

## Conclusions

### What CAN Be Changed (Optional)

1. **Date/time functions** → `IUtil` methods
   - Cleaner API, access to timezone features
   - No functional change (internally uses legacy)

2. **Parameter block building** → `IXpbBuilder`
   - Type-safe, structured approach
   - Better than manual byte array construction

3. **Info buffer parsing** → `ClumpletReader` / `IXpbBuilder` reader
   - Structured parsing with automatic byte order conversion

### What CANNOT Be Changed

1. **`isc_info_*` constants** - No replacements exist
2. **`isc_tpb_*` constants** - No replacements exist
3. **`isc_spb_*` constants** - No replacements exist
4. **`isc_dpb_*` constants** - No replacements exist

These constants are fundamental to the Firebird protocol and are used by both legacy and modern APIs.

## Recommendations for php-firebird Extension

### Phase 1: Current Status - ACCEPTABLE ✅

The current usage of `isc_*` constants in php-firebird is **legitimate and correct**:
- `isc_info_*` constants for database/statement/transaction info
- `isc_tpb_*` constants for transaction parameters
- `isc_spb_*` constants for service parameters
- `isc_encode_*`/`isc_decode_*` for date/time conversion
- `isc_vax_integer()` for byte order conversion

**No migration required** - these are the standard Firebird API constants.

### Phase 2: Optional Future Improvements

If modernization is desired (not required):

1. **Date/time handling** (Low priority)
   - Replace `isc_encode_timestamp()` → `IUtil::encodeDate()` + `IUtil::encodeTime()`
   - Replace `isc_decode_timestamp()` → `IUtil::decodeDate()` + `IUtil::decodeTime()`
   - Add timezone support via `IUtil::encodeTimeStampTz()`, etc.

2. **Parameter block building** (Low priority)
   - Replace manual TPB/DPB/SPB construction with `IXpbBuilder`
   - Cleaner, more maintainable code

3. **Info buffer parsing** (Low priority)
   - Use `ClumpletReader` instead of manual `isc_vax_integer()` calls
   - Structured, type-safe parsing

### Summary

The `isc_*` constants used in php-firebird are **not legacy** - they are the **standard Firebird constants** used by both the legacy C API and the modern C++ OO API. No migration is necessary or recommended for these constants.

The modernization path involves using the structured builder/reader interfaces (`IXpbBuilder`, `ClumpletReader`) rather than replacing constants, but this is optional and provides only code clarity benefits, not functional improvements.
