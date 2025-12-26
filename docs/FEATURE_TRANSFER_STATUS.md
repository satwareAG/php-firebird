# Feature Transfer Status: mlazdans/firebird-php → satwareAG/php-firebird

**Last Updated**: 2025-12-21
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
| Limbo Transaction Functions | ✅ Complete | C | Low |
| Rich Connection Info | ✅ Complete | C | Medium |
| SQLSTATE Exception Codes | ✅ Complete | C | Medium |
| Fetch Date Objects (DateTimeImmutable) | ✅ Complete | C | Low |
| IBatch API | ✅ Complete | C | High |

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

### 7. Limbo Transaction Functions ✅

**Priority**: Low
**Status**: ✅ Complete (December 21, 2025)
**Effort**: 4-8 hours → **Completed in ~30 minutes** (AI-assisted)
**Test**: `tests/fbird_limbo_trans_001.phpt`

For recovering from failed two-phase commit scenarios.

**API**:
```php
// Get list of limbo transaction IDs
$ids = fbird_get_limbo_transactions($db, 100);  // max 100 IDs (default)

// Reconnect to a limbo transaction
$trans = fbird_reconnect_transaction($db, $transaction_id);
fbird_commit($trans);  // or fbird_rollback($trans)
```

**PHP Functions** (`firebird.c`):
- `fbird_get_limbo_transactions(?resource $link, int $max_count = 100): array|false`
  - Returns array of limbo transaction IDs
  - Validates max_count (1-10000 range)
  - Uses default connection if link is null
- `fbird_reconnect_transaction(resource $link, int $transaction_id): resource|false`
  - Reconnects to limbo transaction
  - Returns transaction resource compatible with fbird_commit/rollback
  - Links transaction into connection's transaction list

**C++ Layer Implementation** (`firebird_utils.cpp`):
- `fbt_get_limbo_transactions()` - Queries `isc_info_limbo` via `IAttachment::getInfo()`, parses response buffer
- `fbt_reconnect()` - Uses `IAttachment::reconnectTransaction()`, wraps in `fb::Transaction`

**Implementation Details**:
- Arginfo structures defined for type safety
- Registered in `zend_function_entry` array
- Proper error handling with status vector propagation
- Resource lifecycle management via transaction list

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

### 10. Fetch Date as DateTimeImmutable ✅

**Implemented**: December 20, 2025
**Test**: `tests/fbird_fetch_date_obj_001.phpt`

Option to return date/time columns as DateTimeImmutable objects.

**Constant**:
- `FBIRD_FETCH_DATE_OBJ` (8) - Return dates as DateTimeImmutable

**Usage**:
```php
$row = fbird_fetch_assoc($result, FBIRD_FETCH_DATE_OBJ);
// $row['created_at'] instanceof DateTimeImmutable
// Works with DATE, TIME, and TIMESTAMP columns
// NULL values remain null

$row['DATE_COL']->format('Y-m-d');        // "2025-12-20"
$row['TIME_COL']->format('H:i:s');        // "14:30:45"
$row['TIMESTAMP_COL']->format('Y-m-d H:i:s');  // "2025-12-20 14:30:45"
```

**Notes**:
- Works with `fbird_fetch_assoc()`, `fbird_fetch_row()`, and `fbird_fetch_object()`
- TIME columns use epoch date (1970-01-01) for DateTimeImmutable
- Can be combined with other flags (e.g., `FBIRD_FETCH_BLOBS | FBIRD_FETCH_DATE_OBJ`)

---

## C++ Layer Complete (Phase 3)

### 11. IBatch API ✅

**Priority**: High
**Status**: ✅ Complete (December 20-21, 2025)
**Tests**: 6 tests passing (100%)
**Effort**: 40+ hours estimate → **~8 hours actual** (AI-assisted)
**Research Document**: [IBATCH_API_RESEARCH.md](IBATCH_API_RESEARCH.md)

Bulk operations for significant performance improvements (10-12x speedup for INSERT).

**Test Coverage**:
- `tests/fbird_batch_001.phpt` - Basic batch operations
- `tests/fbird_batch_blob_001.phpt` - BLOB operations (add_blob, register_blob)
- `tests/fbird_batch_errors_001.phpt` - Error reporting
- `tests/fbird_batch_multitype_001.phpt` - Comprehensive multi-type with NULL handling
- `tests/fbird_batch_oo_001.phpt` - OO wrapper (Batch, BatchResult, BatchError classes)
- `tests/blobid_001.phpt` - BlobId value object

**Procedural API**:
```php
// Prepare statement
$stmt = fbird_prepare($db, "INSERT INTO table (col1, col2, blob_col) VALUES (?, ?, ?)");

// Create batch
$batch = fbird_batch_create($stmt, $trans);

// Create inline BLOB
$blob_id = fbird_batch_add_blob($batch, "BLOB content");

// Add rows
fbird_batch_add($batch, 'value1', 'value2', $blob_id);

// Execute and get results
$result = fbird_batch_execute($batch);
echo "Processed: {$result['total_processed']}\n";
echo "Success: {$result['success_count']}\n";
echo "Errors: {$result['error_count']}\n";
```

**OO Wrapper API** (`src/Firebird/`):
```php
use Firebird\Batch;
use Firebird\BatchResult;
use Firebird\BatchError;

// Create batch with fluent interface
$batch = Batch::fromQuery($query, $trans);
$batch->add('value1', 'value2')
      ->add('value3', 'value4')
      ->add('value5', 'value6');

// Execute and get typed result
$result = $batch->execute();  // Returns BatchResult

if ($result->hasErrors()) {
    foreach ($result as $error) {  // Iterate BatchError objects
        echo "Row {$error->position}: {$error->message}\n";
    }
}

echo $result->getSummary();  // "3 rows: 3 succeeded, 0 failed (100.0%)"
```

**PHP Functions** (`firebird.c`):
- `fbird_batch_create($query [, $trans])` - Create batch from prepared statement
- `fbird_batch_add($batch, ...$params)` - Add row with parameter binding
- `fbird_batch_add_blob($batch, $data [, $type])` - Create inline BLOB, returns BLOB ID
- `fbird_batch_register_blob($batch, $blob_id)` - Register existing BLOB for batch use
- `fbird_batch_execute($batch)` - Execute batch, returns ['total_processed', 'success_count', 'error_count']
- `fbird_batch_cancel($batch)` - Cancel without executing

**PHP Classes**:
- `Firebird\Batch` - Main batch wrapper with fluent interface (`fromQuery()`, `add()`, `execute()`)
- `Firebird\BatchResult` - Result container (Countable, IteratorAggregate)
- `Firebird\BatchError` - Per-row error details value object

**Supported SQL Types**:
- Integers: INTEGER, BIGINT, SMALLINT
- Floating: FLOAT, DOUBLE PRECISION
- Fixed-point: NUMERIC(p,s), DECIMAL(p,s)
- Strings: CHAR (with padding), VARCHAR
- Date/Time: DATE, TIME, TIMESTAMP
- Boolean: BOOLEAN (Firebird 3.0+)
- BLOBs: TEXT and BINARY subtypes

**C++ Layer** (`firebird_utils.cpp`):
- `fbbatch_create()` - Uses IXpbBuilder::BATCH with TAG_BLOB_POLICY
- `fbbatch_add()`, `fbbatch_add_blob()`, `fbbatch_register_blob()` - Row/BLOB operations
- `fbbatch_execute()` - Parses IBatchCompletionState for detailed results
- `BatchWrapper` class for RAII resource management

**Requirements**:
- Firebird 4.0+ (FB_API_VER >= 40) - runtime detection via function_exists()
- Transaction integration - batch executes in specified transaction context

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

### Phase 2: Enhanced Features (1 week) ✅
3. **fbird_connection_info()** - ✅ Complete (December 20, 2025)
4. **FBIRD_FETCH_DATE_OBJ** - ✅ Complete (December 20, 2025)

### Phase 3: Advanced Features (2+ weeks) 🔨 C++ Layer Complete
5. **Limbo Transaction Functions** - 🔨 C++ Layer Done (December 20, 2025)
   - `fbt_get_limbo_transactions()`, `fbt_reconnect()` in `firebird_utils.cpp`
   - Remaining: PHP layer in `firebird.c`
6. **IBatch API** - 🔨 C++ Layer Done (December 20, 2025)
   - `fbbatch_create()`, `fbbatch_add()`, `fbbatch_execute()`, etc. in `firebird_utils.cpp`
   - Remaining: PHP layer in `firebird.c`

**AI-Assisted Development Results**:
- Original Estimate: 40+ hours for IBatch alone
- Actual Time: ~1.5 hours total for both features (C++ layer)
- Time Reduction: **96%** using AI-assisted deep research and implementation

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
