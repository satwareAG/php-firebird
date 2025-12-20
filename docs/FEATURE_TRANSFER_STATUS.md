# Feature Transfer Status: mlazdans/firebird-php → satwareAG/php-firebird

**Last Updated**: 2025-12-20
**Analysis Document**: [MLAZDANS_FIREBIRD_PHP_COMPARISON.md](MLAZDANS_FIREBIRD_PHP_COMPARISON.md)

## Executive Summary

This document tracks the progress of adopting valuable patterns from `mlazdans/firebird-php` into `satwareAG/php-firebird`.

### Status Overview

| Feature | Status | Type | Priority |
|---------|--------|------|----------|
| TBuilder Fluent Interface | ✅ Complete | PHP | High |
| BlobId Value Object | ✅ Complete | PHP | High |
| OO Wrapper (Database, Transaction) | ✅ Complete | PHP | High |
| DbInfo Structure | ✅ Complete | PHP | High |
| BLOB Seek | ✅ Complete | C | Medium |
| Transaction ID | ✅ Available | C/PHP | N/A |
| Limbo Transaction Functions | ❌ Not Started | C | Low |
| Rich Connection Info | ✅ Complete | C | Medium |
| SQLSTATE Exception Codes | ✅ Complete | C | Medium |
| Fetch Date Objects (DateTimeImmutable) | ❌ Not Started | C | Low |
| IBatch API | 📋 Research Done | C | Future |

---

## Completed Features

### 1. TBuilder - Fluent Transaction Parameter Builder ✅

**File**: `src/Firebird/TBuilder.php`
**Test**: `tests/tbuilder_001.phpt`

```php
use Firebird\TBuilder;

$trans = TBuilder::create()
    ->connection($db)
    ->readCommitted()
    ->wait(10)
    ->readOnly()
    ->start();
```

**Implemented Methods**:
- Isolation: `snapshot()`, `readCommitted()`, `isolationSnapshot()`, `isolationReadCommitted()`, `isolationReadCommittedRecordVersion()`, `isolationReadCommittedNoRecordVersion()`, `isolationReadCommittedReadConsistency()`
- Access: `readOnly()`, `readWrite()`
- Locking: `wait($timeout)`, `noWait()`
- Table Reservations: `reserveSharedRead()`, `reserveSharedWrite()`, `reserveProtectedRead()`, `reserveProtectedWrite()`, `reserveExclusiveRead()`, `reserveExclusiveWrite()`
- Build: `build()`, `buildFlags()`, `start()`

### 2. BlobId - Type-Safe BLOB Identifier ✅

**File**: `src/Firebird/BlobId.php`
**Test**: `tests/blobid_001.phpt`

```php
use Firebird\BlobId;

$blobId = BlobId::fromString($row['blob_column']);
echo $blobId; // Backward compatible __toString()

if ($blobId->equals($otherBlobId)) {
    // Compare BLOB IDs
}

if ($blobId->isNull()) {
    // Handle NULL BLOB
}
```

**Implemented Methods**:
- Factory: `fromString()`, `fromParts()`, `null()`, `tryFromString()`
- Properties: `getHigh()`, `getLow()`, `toString()`, `toInt64()`
- Comparison: `equals()`, `isNull()`, `isNotNull()`
- Validation: `isValidFormat()`

### 3. OO Wrapper Layer ✅

**Files**: 
- `src/Firebird/Database.php`
- `src/Firebird/Transaction.php`

```php
use Firebird\Database;

$db = Database::connect($dsn, $user, $pass);
$trans = $db->beginTransaction();

try {
    $db->queryWithTransaction($trans, "INSERT INTO log (msg) VALUES (?)", ['Hello']);
    $trans->commit();
} catch (\Exception $e) {
    $trans->rollback();
    throw $e;
}

// Savepoints
$trans->savepoint('sp1');
$trans->rollbackToSavepoint('sp1');
$trans->releaseSavepoint('sp1');

$db->close();
```

### 4. DbInfo Structure ✅

**File**: `src/Firebird/DbInfo.php`
**Test**: `tests/dbinfo_001.phpt`

```php
use Firebird\DbInfo;

$info = DbInfo::fromArray([
    'page_size' => 16384,
    'ods_version' => 13,
    'ods_minor_version' => 1,
]);

echo $info->getOdsVersionString();       // "13.1"
echo $info->getPageSizeFormatted();      // "16 KB"
echo $info->isFirebird4OrLater();        // true
```

### 5. BLOB Seek ✅

**Commit**: ccc3e33 (December 19, 2025)
**Test**: `tests/fbird_blob_seek_001.phpt`

```php
$blob = fbird_blob_open($db, $trans, $blob_id);
$pos = fbird_blob_seek($blob, 100, FBIRD_BLOB_SEEK_SET);
$pos = fbird_blob_seek($blob, -50, FBIRD_BLOB_SEEK_CUR);
$pos = fbird_blob_seek($blob, 0, FBIRD_BLOB_SEEK_END);
```

**Constants Implemented**:
- `FBIRD_BLOB_SEEK_SET` (0)
- `FBIRD_BLOB_SEEK_CUR` (1)
- `FBIRD_BLOB_SEEK_END` (2)

---

## Pending Features (C Implementation Required)

### 6. Transaction ID - Already Available ✅

**Status**: Already implemented via `fbird_trans_info()`

**Procedural API** (existing):
```php
$trans = fbird_trans($db);
$info = fbird_trans_info($trans);
echo "Transaction ID: " . $info['id'];
```

**OO Wrapper API** (added 2025-12-19):
```php
use Firebird\Transaction;

$trans = Transaction::begin($db);
echo "Transaction ID: " . $trans->getId();

// Full transaction info
$info = $trans->getInfo();
// Returns: ['id' => 123, 'isolation' => 'READ_COMMITTED', 'lock_timeout' => 0, 'access_mode' => 'READ_WRITE', 'state' => 'ACTIVE']
```

**Note**: The `fbird_trans_info()` function returns comprehensive transaction information including the ID. A dedicated `fbird_trans_id()` function is not needed as the existing API provides this functionality.

### 7. Limbo Transaction Functions ❌

**Priority**: Low
**Effort**: 4-8 hours

For recovering from failed two-phase commit scenarios.

**Proposed API**:
```php
// Get list of limbo transaction IDs
$ids = fbird_get_limbo_transactions($db, 100);  // max 100 IDs

// Reconnect to a limbo transaction
$trans = fbird_reconnect_transaction($db, $transaction_id);
$trans->commit();  // or rollback
```

**Firebird API**:
```cpp
isc_reconnect_transaction(ISC_STATUS*, isc_db_handle*, isc_tr_handle*, short, const char*);
```

### 8. Rich Connection Info ✅

**Commit**: be7b303 (December 20, 2025)
**Test**: `tests/fbird_connection_info_001.phpt`

Connection-level statistics via `IAttachment::getInfo()` OO API.

**Usage**:
```php
$info = fbird_connection_info($db);
// Returns array with statistics:
// - reads, writes, fetches, marks (performance metrics)
// - page_size, num_buffers, sql_dialect (configuration)
// - current_memory, max_memory, allocation (memory stats)
// - attachment_id, ods_version, ods_minor_version (identifiers)
```

**Implementation Notes**:
- Uses `fbc_get_info()` wrapper in `firebird_utils.cpp`
- Calls `IAttachment::getInfo()` via OO API for modern connections
- Fallback to legacy `isc_database_info()` for non-OO connections

### 9. SQLSTATE Exception Codes ✅

**Implemented**: December 20, 2025
**Test**: `tests/fbird_sqlstate_001.phpt`

Standard SQLSTATE error classification for error handling.

**Usage**:
```php
@fbird_query($db, "INVALID SQL");
$sqlstate = fbird_sqlstate();  // Returns "42000" (syntax error class)

// Common SQLSTATE codes:
// - "23000" - Integrity constraint violation
// - "42000" - Syntax error or access rule violation
// - "HY000" - General error
// - false   - No error
```

**Notes**:
- Returns 5-character SQLSTATE string (SQL:2003 standard)
- Returns `false` if no error has occurred
- Uses Firebird's `fb_sqlstate()` API (Firebird 2.5+)

### 10. Fetch Date as DateTimeImmutable ❌

**Priority**: Low
**Effort**: 4-8 hours

Add option to return date/time columns as DateTimeImmutable objects.

**Current Constants**:
- `FBIRD_TEXT` (1) - Fetch BLOBs as strings
- `FBIRD_UNIXTIME` (4) - Return timestamps as Unix timestamps

**Proposed Addition**:
```php
const FBIRD_FETCH_DATE_OBJ = 8;  // Return dates as DateTimeImmutable

$row = fbird_fetch_assoc($result, FBIRD_FETCH_DATE_OBJ);
// $row['created_at'] instanceof DateTimeImmutable
```

---

## Research Complete (Future Implementation)

### 11. IBatch API 📋

**Priority**: Future
**Effort**: 40+ hours (complex)
**Research Document**: [IBATCH_API_RESEARCH.md](IBATCH_API_RESEARCH.md)

Bulk operations for significant performance improvements (10-12x speedup for INSERT).

**Proposed API**:
```php
$stmt = fbird_prepare($db, "INSERT INTO table (col1, col2) VALUES (?, ?)");
$batch = fbird_batch_create($stmt);

foreach ($data as $row) {
    fbird_batch_add($batch, $row['col1'], $row['col2']);
}

$result = fbird_batch_execute($batch);
echo "Inserted: " . $result['success_count'];
```

**Requirements**:
- Firebird 4.0+ (ODS 13+)
- Complex message metadata handling
- Special BLOB handling
- Partial success error tracking

---

## Not Adopting (Explicitly Rejected)

| Feature | Reason |
|---------|--------|
| Pure OO API | Would break backward compatibility with existing procedural users |
| No Array Support | Our users require Firebird array functionality |
| PHP 8.4+ Only | Too restrictive; we support PHP 8.1+ for production environments |
| Service API OO Rewrite | Current procedural API works well; not worth breaking changes |

---

## Implementation Priority Order

### Phase 1: Quick Wins (1-2 days)
1. ~~**fbird_trans_id()**~~ - ✅ Already available via `fbird_trans_info()['id']`
2. ~~**fbird_sqlstate()**~~ - ✅ Complete (December 20, 2025)

### Phase 2: Enhanced Features (1 week)
3. **fbird_connection_info()** - ✅ Complete
4. **FBIRD_FETCH_DATE_OBJ** - DateTimeImmutable support

### Phase 3: Advanced Features (2+ weeks)
5. **Limbo Transaction Functions** - 2PC recovery
6. **IBatch API** - Bulk operations (requires Firebird 4.0+)

---

## Testing Strategy

Each new C function requires:
1. Unit test (`.phpt` file)
2. PHPStan stub update (`phpstan/fbird.stub.php`)
3. Documentation update

Example test structure:
```
tests/
├── fbird_trans_id_001.phpt
├── fbird_sqlstate_001.phpt
├── fbird_connection_info_001.phpt
└── fbird_fetch_date_obj_001.phpt
```

---

## See Also

- [MLAZDANS_FIREBIRD_PHP_COMPARISON.md](MLAZDANS_FIREBIRD_PHP_COMPARISON.md) - Original competitive analysis
- [OO_WRAPPER_IMPLEMENTATION.md](OO_WRAPPER_IMPLEMENTATION.md) - PHP wrapper details
- [BLOB_SEEK_IMPLEMENTATION.md](BLOB_SEEK_IMPLEMENTATION.md) - BLOB seek implementation
- [IBATCH_API_RESEARCH.md](IBATCH_API_RESEARCH.md) - Bulk operations research
