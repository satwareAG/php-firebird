# OO Wrapper Layer Implementation

## Overview

This document summarizes the PHP OO wrapper layer implementation for php-firebird, adopting patterns from the mlazdans/firebird-php project while maintaining full backward compatibility.

## Implementation Status (2025-12-19)

All 6 planned features have been implemented in PHP:

| Feature | Status | File | Description |
|---------|--------|------|-------------|
| TBuilder | ✅ Complete | `src/Firebird/TBuilder.php` | Fluent transaction parameter builder |
| BlobId | ✅ Complete | `src/Firebird/BlobId.php` | Type-safe BLOB identifier value object |
| BLOB Seek | 📋 Documented | `docs/BLOB_SEEK_IMPLEMENTATION.md` | C implementation plan for fbird_blob_seek() |
| DbInfo | ✅ Complete | `src/Firebird/DbInfo.php` | Rich database information structure |
| IBatch | 📋 Documented | `docs/IBATCH_API_RESEARCH.md` | Research for bulk operations API |
| OO Wrapper | ✅ Complete | `src/Firebird/Database.php`, `Transaction.php` | OO wrapper layer |

## Files Created

### PHP Classes (`src/Firebird/`)

```
src/Firebird/
├── BlobId.php        # BLOB ID value object
├── Database.php      # OO database connection wrapper
├── DbInfo.php        # Database information structure
├── TBuilder.php      # Transaction parameter builder
└── Transaction.php   # OO transaction wrapper
```

### Tests (`tests/`)

```
tests/
├── tbuilder_001.phpt    # TBuilder fluent API tests
├── blobid_001.phpt      # BlobId value object tests
└── dbinfo_001.phpt      # DbInfo structure tests
```

### Documentation (`docs/`)

```
docs/
├── BLOB_SEEK_IMPLEMENTATION.md      # BLOB seek C implementation plan
├── IBATCH_API_RESEARCH.md           # IBatch API research
├── MLAZDANS_FIREBIRD_PHP_COMPARISON.md  # Original comparison document
└── OO_WRAPPER_IMPLEMENTATION.md     # This summary
```

## Feature Details

### 1. TBuilder - Transaction Parameter Builder

Fluent interface for building transaction parameters (TPB):

```php
use Firebird\TBuilder;

// Method 1: Using start() with connection
$trans = TBuilder::create()
    ->connection($db)
    ->readCommitted()
    ->wait(10)
    ->readOnly()
    ->start();

// Method 2: Using build() for manual control
$options = TBuilder::create()
    ->readCommitted()
    ->wait(5)
    ->build();
$trans = fbird_trans_start($db, $options);
```

**Supported Methods:**
- Connection: `connection()` → set connection for start(), `start()` → begin transaction
- Isolation: `snapshot()`, `readCommitted()`, `recordVersion()` (convenience aliases)
- Full isolation: `isolationSnapshot()`, `isolationReadCommitted()`, `isolationReadCommittedRecordVersion()`, `isolationReadCommittedNoRecordVersion()`, `isolationReadCommittedReadConsistency()`
- Options: `readOnly()`, `readWrite()`, `wait()`, `noWait()`, `ignoreLimbo()`, `autoCommit()`, `noAutoUndo()`
- Table locks: `reserveSharedRead()`, `reserveSharedWrite()`, `reserveProtectedRead()`, `reserveProtectedWrite()`, `reserveExclusiveRead()`, `reserveExclusiveWrite()`
- Build: `build()` → returns options array, `buildFlags()` → returns bitmask, `start()` → starts transaction
- Utility: `reset()`, `resetAll()`, `copy()`, `hasConnection()`, `getLockTimeout()`, `getTableReservations()`

### 2. BlobId - Type-Safe BLOB Identifier

Value object for type-safe BLOB ID handling:

```php
use Firebird\BlobId;

$blobId = BlobId::fromString($row['blob_column']);
echo $blobId;  // String representation (backward compatible)

if ($blobId->equals($otherBlobId)) {
    // Compare BLOB IDs
}
```

**Features:**
- Immutable value object
- `__toString()` for backward compatibility
- `fromString()` factory method
- `equals()` comparison
- `isNull()` check

### 3. BLOB Seek Support (C Implementation Required)

Documented implementation plan for `fbird_blob_seek()`:

```php
// Desired API (requires C implementation)
$pos = fbird_blob_seek($blob, 100, FBIRD_BLOB_SEEK_SET);
$pos = fbird_blob_seek($blob, -50, FBIRD_BLOB_SEEK_CURRENT);
$pos = fbird_blob_seek($blob, 0, FBIRD_BLOB_SEEK_END);
```

**Requirements:**
- Add C wrapper in `firebird_utils.cpp` using `IBlob::seek()`
- Add PHP function in `fbird_blobs.c`
- Register constants in `firebird.c`
- Only works with stream BLOBs (subtype 1)

See `docs/BLOB_SEEK_IMPLEMENTATION.md` for complete implementation guide.

### 4. DbInfo - Rich Database Information

Structured access to database metadata:

```php
use Firebird\DbInfo;

$info = DbInfo::fromArray([
    'page_size' => 16384,
    'ods_version' => 13,
    'ods_minor_version' => 1,
    'current_memory' => 16777216,
]);

echo $info->getOdsVersionString();        // "13.1"
echo $info->getPageSizeFormatted();       // "16 KB"
echo $info->isFirebird4OrLater();         // true
```

**Note:** Full functionality requires `fbird_connection_info()` C implementation to retrieve live database stats.

### 5. IBatch API (Research Complete)

Research document for Firebird 4.0+ bulk operations:

```php
// Future API (requires C implementation)
$batch = fbird_batch_create($stmt);
foreach ($rows as $row) {
    fbird_batch_add($batch, $row['col1'], $row['col2']);
}
$result = fbird_batch_execute($batch);
```

**Performance:** 10-12x speedup for bulk INSERT operations.

See `docs/IBATCH_API_RESEARCH.md` for complete research and phased implementation plan.

### 6. OO Wrapper Layer

Modern object-oriented interface wrapping procedural functions:

```php
use Firebird\Database;

// Connect
$db = Database::connect(
    'localhost:/var/lib/firebird/test.fdb',
    'SYSDBA',
    'masterkey'
);

// Transaction with builder
$trans = $db->transaction()
    ->readCommitted()
    ->wait()
    ->start();

// Query with transaction
$result = $db->queryWithTransaction($trans, 
    "SELECT * FROM users WHERE id = ?", [1]);

// Commit
$trans->commit();

// Close
$db->close();
```

**Classes:**
- `Database` - Connection management, query execution, BLOB operations
- `Transaction` - Transaction lifecycle, savepoints, commit/rollback

**Interoperability:**
```php
// Get underlying resource for procedural functions
$resource = $db->getResource();
$result = fbird_query($resource, "SELECT * FROM users");
```

## Design Principles

### 1. Backward Compatibility

All new features wrap existing procedural functions:
- OO classes call `fbird_*()` functions internally
- `getResource()` methods expose underlying resources
- Can mix procedural and OO code in same application

### 2. PHP 8.1+ Compatibility

Uses modern PHP features:
- `readonly` properties for immutability
- `mixed` type for resources
- Union types where appropriate
- Constructor promotion

### 3. Array Support Maintained

Unlike mlazdans which dropped array support:
- All existing array functionality preserved
- TPB arrays still accepted alongside TBuilder
- Full backward compatibility with existing code

### 4. Namespace Isolation

All new classes under `Firebird\` namespace:
- `Firebird\Database`
- `Firebird\Transaction`
- `Firebird\TBuilder`
- `Firebird\BlobId`
- `Firebird\DbInfo`

No collision with existing global functions.

## Usage Examples

### Basic OO Usage

```php
use Firebird\Database;

$db = Database::connect($dsn, $user, $pass);
$trans = $db->beginTransaction();

try {
    $db->queryWithTransaction($trans, 
        "INSERT INTO log (message) VALUES (?)", ['Hello']);
    $trans->commit();
} catch (\Exception $e) {
    $trans->rollback();
    throw $e;
}

$db->close();
```

### Mixed Procedural/OO

```php
use Firebird\Database;
use Firebird\TBuilder;

// OO connection
$db = Database::connect($dsn, $user, $pass);

// Procedural query with OO transaction
$trans = TBuilder::create()
    ->connection($db->getResource())
    ->readCommitted()
    ->start();

// Mix styles freely
$result = fbird_query($db->getResource(), $trans->getResource(), 
    "SELECT * FROM users");

while ($row = fbird_fetch_assoc($result)) {
    // Process row
}

fbird_commit($trans->getResource());
$db->close();
```

### Savepoints

```php
$trans = $db->beginTransaction();

$trans->savepoint('sp1');
$db->queryWithTransaction($trans, "INSERT INTO ...");

$trans->savepoint('sp2');
$db->queryWithTransaction($trans, "INSERT INTO ...");

// Undo sp2 changes
$trans->rollbackToSavepoint('sp2');

// Keep sp1 changes
$trans->commit();
```

## Future Work

### C Code Implementation Required

1. **BLOB Seek** - `fbird_blob_seek()` in `fbird_blobs.c`
2. **Rich DbInfo** - `fbird_connection_info()` returning full stats
3. **IBatch** - Complete batch API for bulk operations

### PHP Wrapper Extensions

1. **Statement Class** - Wrap prepared statement lifecycle
2. **Result Class** - Wrap result iteration
3. **Blob Class** - Wrap BLOB stream operations

## Testing

Run the new tests:

```bash
# Individual test files
php -d extension=./modules/firebird.so tests/tbuilder_001.phpt
php -d extension=./modules/firebird.so tests/blobid_001.phpt
php -d extension=./modules/firebird.so tests/dbinfo_001.phpt

# Full test suite
scripts/container/test.sh
```

## See Also

- [MLAZDANS_FIREBIRD_PHP_COMPARISON.md](MLAZDANS_FIREBIRD_PHP_COMPARISON.md) - Original feature comparison
- [BLOB_SEEK_IMPLEMENTATION.md](BLOB_SEEK_IMPLEMENTATION.md) - C implementation plan
- [IBATCH_API_RESEARCH.md](IBATCH_API_RESEARCH.md) - Bulk operations research
