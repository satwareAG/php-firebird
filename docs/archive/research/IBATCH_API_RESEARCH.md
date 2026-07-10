# IBatch API Implementation

## Overview

The IBatch interface in Firebird 4.0+ provides high-performance bulk operations for inserting large volumes of data. This document tracks the implementation status and usage of IBatch support in php-firebird.

## Implementation Status

**Status:** ✅ **COMPLETE (Full Implementation)**

**Completion Date:** 2025-12-21

**All Phases Complete:**
- ✅ Phase 3: Batch resource infrastructure (create, execute, cancel)
- ✅ Phase 3: Parameter binding to message buffers
- ✅ Phase 3: Full type conversion (integers, floats, strings, dates, nullable types)
- ✅ Phase 3: Transaction integration
- ✅ Phase 3: BLOB handling (addBlob, registerBlob with "HHHHHHHH:LLLL" format)
- ✅ Phase 4: Error reporting (total_processed, success_count, error_count)
- ✅ Phase 5: PHP OO wrapper (Firebird\Batch, BatchResult, BatchError classes)
- ✅ Phase 6: Comprehensive multi-type tests with NULL handling

**Tests (6/6 passing - 100%):**
- ✅ `tests/blobid_001.phpt` - BlobId value object
- ✅ `tests/fbird_batch_001.phpt` - Basic batch operations
- ✅ `tests/fbird_batch_blob_001.phpt` - BLOB operations (add_blob, register_blob)
- ✅ `tests/fbird_batch_errors_001.phpt` - Error reporting and success_count
- ✅ `tests/fbird_batch_multitype_001.phpt` - Comprehensive multi-type with NULL handling
- ✅ `tests/fbird_batch_oo_001.phpt` - OO wrapper classes

## Firebird IBatch Interface

### Location in Firebird API

The IBatch interface is part of Firebird's OO API (FB_API_VER >= 40):

```cpp
// From firebird/Interface.h
class IBatch : public IReferenceCounted {
public:
    // Message operations
    void add(IStatus* status, unsigned count, const void* inBuffer);
    void addBlob(IStatus* status, unsigned length, const void* inBuffer,
                 ISC_QUAD* blobId, unsigned bpbLength, const unsigned char* bpb);
    void appendBlobData(IStatus* status, unsigned length, const void* inBuffer);
    void addBlobStream(IStatus* status, unsigned length, const void* inBuffer);
    
    // Registration
    unsigned registerBlob(IStatus* status, const ISC_QUAD* existingBlob,
                         ISC_QUAD* blobId);
    
    // Execution
    IBatchCompletionState* execute(IStatus* status);
    void cancel(IStatus* status);
    
    // Information
    unsigned getBlobAlignment(IStatus* status);
    IMessageMetadata* getMetadata(IStatus* status);
    void setDefaultBpb(IStatus* status, unsigned parLength, const unsigned char* par);
    void close(IStatus* status);
};
```

### IBatchCompletionState Interface

```cpp
class IBatchCompletionState : public IDisposable {
public:
    // Get total size after execution
    unsigned getSize(IStatus* status);
    
    // Get state of individual record
    int getState(IStatus* status, unsigned pos);
    
    // Find next error record
    unsigned findError(IStatus* status, unsigned pos);
    
    // Get detailed status for error
    void getStatus(IStatus* status, IStatus* to, unsigned pos);
};
```

### Creating IBatch

From IStatement/IAttachment:

```cpp
// From prepared statement
IBatch* IStatement::createBatch(IStatus* status,
    IMessageMetadata* inMetadata,
    unsigned parLength, const unsigned char* par);

// Direct from attachment (Firebird 5.0+)
IBatch* IAttachment::createBatch(IStatus* status,
    ITransaction* transaction,
    unsigned stmtLength, const char* sqlStmt,
    unsigned dialect,
    IMessageMetadata* inMetadata,
    unsigned parLength, const unsigned char* par);
```

## Use Cases

### 1. Bulk INSERT Operations

The primary use case - inserting many rows efficiently:

```php
// Desired PHP API
$stmt = fbird_prepare($db, "INSERT INTO table (col1, col2) VALUES (?, ?)");
$batch = fbird_batch_create($stmt, [
    'count' => 1000,  // Batch buffer size
]);

foreach ($data as $row) {
    fbird_batch_add($batch, $row['col1'], $row['col2']);
}

$result = fbird_batch_execute($batch);
echo "Inserted: " . $result['success_count'] . " rows\n";
echo "Errors: " . $result['error_count'] . "\n";
```

### 2. Bulk INSERT with BLOBs

```php
$stmt = fbird_prepare($db, "INSERT INTO docs (name, content) VALUES (?, ?)");
$batch = fbird_batch_create($stmt);

foreach ($files as $file) {
    $blobId = fbird_batch_add_blob($batch, file_get_contents($file['path']));
    fbird_batch_add($batch, $file['name'], $blobId);
}

fbird_batch_execute($batch);
```

## Performance Benefits

According to Firebird documentation:

| Operation | Traditional | IBatch | Speedup |
|-----------|-------------|--------|---------|
| 10,000 INSERTs | ~500ms | ~50ms | 10x |
| 100,000 INSERTs | ~5,000ms | ~400ms | 12x |
| 1M INSERTs | ~50,000ms | ~4,000ms | 12x |

Benefits:
- Reduced network round-trips (single batch vs many individual statements)
- Server-side optimization (prepared once, executed many)
- Efficient BLOB handling (stream-based, not ID-based)
- Automatic error handling with partial success tracking

## Implementation Plan

### Phase 1: C Wrapper Functions

Add to `firebird_utils.cpp`:

```cpp
// Batch creation from statement
void* fbb_batch_create(void* master, void* stmt, void* metadata,
                       unsigned bufferSize, ISC_STATUS* status);

// Add row to batch
int fbb_batch_add(void* master, void* batch, unsigned count,
                  const void* data, ISC_STATUS* status);

// Execute batch
void* fbb_batch_execute(void* master, void* batch, ISC_STATUS* status);

// Get batch completion state
int fbb_batch_get_completion_state(void* master, void* completion,
                                   unsigned* total, unsigned* errors,
                                   ISC_STATUS* status);

// Close batch
void fbb_batch_close(void* master, void* batch, ISC_STATUS* status);
```

### Phase 2: PHP Functions

```c
/* {{{ proto resource fbird_batch_create(resource stmt [, array options])
   Create a batch operation from prepared statement */
PHP_FUNCTION(fbird_batch_create)

/* {{{ proto bool fbird_batch_add(resource batch, mixed ...params)
   Add a row to the batch */
PHP_FUNCTION(fbird_batch_add)

/* {{{ proto array fbird_batch_execute(resource batch)
   Execute the batch and return results */
PHP_FUNCTION(fbird_batch_execute)

/* {{{ proto bool fbird_batch_cancel(resource batch)
   Cancel and close the batch without executing */
PHP_FUNCTION(fbird_batch_cancel)
```

### Phase 3: PHP OO Wrapper

```php
namespace Firebird;

class Batch {
    private mixed $resource;
    private int $rowCount = 0;
    
    public static function create(Statement $stmt, array $options = []): self;
    
    public function add(mixed ...$params): self;
    public function addBlob(string $data): BlobId;
    public function execute(): BatchResult;
    public function cancel(): void;
    public function getRowCount(): int;
}

class BatchResult {
    public readonly int $totalRows;
    public readonly int $successCount;
    public readonly int $errorCount;
    public readonly array $errors;
    
    public function hasErrors(): bool;
    public function getErrorAt(int $position): ?BatchError;
}
```

## Challenges

### 1. Message Metadata

IBatch requires IMessageMetadata for parameter binding:
- Need to extract from prepared statement
- Map PHP types to Firebird types
- Handle nullable parameters

### 2. BLOB Handling

IBatch has special BLOB methods:
- `addBlob()` - inline BLOB data
- `addBlobStream()` - streamed BLOB data  
- `registerBlob()` - reference existing BLOB
- Different from standard BLOB ID binding

### 3. Error Handling

Partial success scenarios:
- Some rows succeed, some fail
- Need to report which rows failed and why
- Complex completion state parsing

### 4. Buffer Management

Fixed buffer size considerations:
- Auto-execute when buffer full?
- User-controlled batch sizes?
- Memory management for large batches

## mlazdans/firebird-php Reference

Their approach (from stub analysis):

```php
// Statement method
public function createBatch(int $pb_size = 0): Batch;

// Batch class
class Batch {
    public function add(mixed ...$message): void;
    public function execute(): bool;
    public function cancel(): void;
    public function addBlob(): int;  // Returns position
    public function appendBlobData(string $data): void;
    public function getBlobAlignment(): int;
    public function getMessageMetadata(): Message_Metadata;
    public function registerBlob(Blob $blob): Blob_Id;
    public function setDefaultBpb(string $bpb): void;
}
```

## Recommendation

### Implementation Priority: Medium-High

**Pros:**
- Significant performance benefit for bulk operations
- Modern API matching Firebird 4.0+ capabilities
- Competitive with mlazdans feature set

**Cons:**
- Complex implementation (message metadata, BLOB handling)
- Requires Firebird 4.0+ (limiting older installations)
- Error handling complexity

### Suggested Approach

1. **Phase 1 (MVP):** Basic batch INSERT without BLOBs
   - `fbird_batch_create($stmt)` 
   - `fbird_batch_add($batch, ...$params)`
   - `fbird_batch_execute($batch)` → returns count only

2. **Phase 2:** Error handling
   - Detailed completion state parsing
   - Error position and message retrieval

3. **Phase 3:** BLOB support
   - `fbird_batch_add_blob()`
   - Stream-based BLOB handling

4. **Phase 4:** PHP OO Wrapper
   - `Firebird\Batch` class
   - `Firebird\BatchResult` and `Firebird\BatchError`

### Version Requirements

- Firebird 4.0+ (ODS 13+) for IBatch support
- Runtime feature detection recommended
- Graceful fallback to traditional INSERT for older versions

## Implemented API

### Core Functions

**fbird_batch_create(resource $query [, resource $trans_identifier]): resource|false**
- Creates a batch from a prepared statement
- Parameters:
  - `$query`: Prepared statement resource from fbird_prepare()
  - `$trans_identifier`: Optional transaction (defaults to query's transaction)
- Returns: Batch resource or false on error

**fbird_batch_add(resource $batch, mixed ...$args): bool**
- Adds a row of parameters to the batch
- Parameters:
  - `$batch`: Batch resource from fbird_batch_create()
  - `...$args`: Variadic parameters matching prepared statement placeholders
- Returns: true on success, false on error
- Supported types: integers, floats, strings, dates, NULL values

**fbird_batch_execute(resource $batch): array|false**
- Executes the batch and returns results
- Returns: Array with keys:
  - `total_processed`: Number of messages processed
  - `error_count`: Number of failed messages
- Note: Batch is automatically closed after execution

**fbird_batch_cancel(resource $batch): bool**
- Cancels batch without executing
- Frees resources without inserting data

### Supported SQL Types

| SQL Type | PHP Input | Binary Format | NULL Support |
|----------|-----------|---------------|--------------|
| SMALLINT | integer | 16-bit signed | ✅ |
| INTEGER | integer | 32-bit signed | ✅ |
| BIGINT | integer | 64-bit signed | ✅ |
| NUMERIC/DECIMAL | float | Scaled integer | ✅ |
| FLOAT | float | 32-bit IEEE | ✅ |
| DOUBLE PRECISION | float | 64-bit IEEE | ✅ |
| CHAR(n) | string | Fixed-length, space-padded | ✅ |
| VARCHAR(n) | string | 2-byte length + data | ✅ |
| DATE | string/int | ISC_DATE | ✅ |
| TIME | string/int | ISC_TIME | ✅ |
| TIMESTAMP | string/int | ISC_TIMESTAMP | ✅ |
| TIME WITH TIME ZONE | string/int | ISC_TIME_TZ (FB 4.0+) | ✅ |
| TIMESTAMP WITH TIME ZONE | string/int | ISC_TIMESTAMP_TZ (FB 4.0+) | ✅ |
| BOOLEAN | bool | FB_BOOLEAN (FB 3.0+) | ✅ |
| BLOB | string (blob ID) | ISC_QUAD | ✅ |

### Usage Example

```php
<?php
// Connect and prepare
$db = fbird_connect('localhost:/path/to/db.fdb', 'SYSDBA', 'masterkey');
$trans = fbird_trans($db);

// Prepare INSERT statement
$stmt = fbird_prepare($trans, 'INSERT INTO customers (id, name, email, created_at) VALUES (?, ?, ?, ?)');

// Create batch
$batch = fbird_batch_create($stmt, $trans);

// Add multiple rows
for ($i = 1; $i <= 10000; $i++) {
    fbird_batch_add($batch, 
        $i,                                    // id (INTEGER)
        "Customer $i",                         // name (VARCHAR)
        "customer{$i}@example.com",           // email (VARCHAR)
        date('Y-m-d H:i:s')                   // created_at (TIMESTAMP)
    );
}

// Execute batch
$result = fbird_batch_execute($batch);
echo "Processed: {$result['total_processed']} rows\n";
echo "Errors: {$result['error_count']}\n";

// Commit transaction
fbird_commit($trans);
fbird_close($db);
```

### NULL Handling

```php
// NULL values are supported for all types
$batch = fbird_batch_create($stmt, $trans);

fbird_batch_add($batch, 1, 'John', 'john@example.com', null); // NULL timestamp
fbird_batch_add($batch, 2, null, 'jane@example.com', time()); // NULL name
fbird_batch_add($batch, null, 'Bob', null, null);             // NULL id, email, timestamp

$result = fbird_batch_execute($batch);
```

### Date/Time Formats

```php
// Accepts unix timestamps (integers)
fbird_batch_add($batch, 1, 'Name', time());

// Or formatted strings
fbird_batch_add($batch, 2, 'Name', '2025-12-21 00:00:00');
fbird_batch_add($batch, 3, 'Name', '2025-12-21');  // DATE only
fbird_batch_add($batch, 4, 'Name', '15:30:45');    // TIME only

// Timezone-aware (Firebird 4.0+)
fbird_batch_add($batch, 5, 'Name', '2025-12-21 15:30:45 Europe/Berlin');
```

### BLOB Functions

**fbird_batch_add_blob(resource $batch, string $data [, int $type = 0]): string|false**
- Creates an inline BLOB within the batch context
- Parameters:
  - `$batch`: Batch resource from fbird_batch_create()
  - `$data`: BLOB content data
  - `$type`: BLOB subtype (0 = BINARY, 1 = TEXT, default 0)
- Returns: BLOB ID string in "HHHHHHHH:LLLL" format (13 characters) or false on error

**fbird_batch_register_blob(resource $batch, string $blob_id): string|false**
- Registers an existing BLOB for use in a batch operation
- Parameters:
  - `$batch`: Batch resource from fbird_batch_create()
  - `$blob_id`: Existing BLOB ID string from fbird_blob_close()
- Returns: Batch-compatible BLOB ID string or false on error

### BLOB Example

```php
<?php
$db = fbird_connect('localhost:/path/to/db.fdb', 'SYSDBA', 'masterkey');
$trans = fbird_trans($db);

// Table with BLOB column
$stmt = fbird_prepare($trans, 'INSERT INTO documents (id, name, content) VALUES (?, ?, ?)');
$batch = fbird_batch_create($stmt, $trans);

// Create inline BLOBs and add rows
$blob1 = fbird_batch_add_blob($batch, 'This is text BLOB content', 1);  // TEXT
$blob2 = fbird_batch_add_blob($batch, file_get_contents('image.png'), 0);  // BINARY

fbird_batch_add($batch, 1, 'Text Document', $blob1);
fbird_batch_add($batch, 2, 'Image File', $blob2);

$result = fbird_batch_execute($batch);
echo "Inserted: {$result['success_count']} documents with BLOBs\n";

fbird_commit($trans);
fbird_close($db);
```

## PHP OO Wrapper Classes

### Firebird\Batch

Main batch class with fluent interface for batch operations.

```php
namespace Firebird;

class Batch {
    /**
     * Create a Batch from a prepared statement.
     * @param resource $query Prepared statement from fbird_prepare()
     * @param resource|null $trans Optional transaction resource
     * @return self
     */
    public static function fromQuery(mixed $query, mixed $trans = null): self;
    
    /**
     * Add a row of parameters to the batch (fluent).
     * @param mixed ...$params Parameters matching prepared statement
     * @return self For method chaining
     */
    public function add(mixed ...$params): self;
    
    /**
     * Execute the batch and return results.
     * @return BatchResult Result object with counts and errors
     */
    public function execute(): BatchResult;
}
```

### Firebird\BatchResult

Result container implementing `Countable` and `IteratorAggregate`.

```php
namespace Firebird;

class BatchResult implements \Countable, \IteratorAggregate {
    public readonly int $totalRows;      // Total rows processed
    public readonly int $successCount;   // Successfully inserted
    public readonly int $errorCount;     // Failed rows
    
    /**
     * Create from fbird_batch_execute() result array.
     */
    public static function fromArray(array $data): self;
    
    public function hasErrors(): bool;           // errorCount > 0
    public function isComplete(): bool;          // successCount == totalRows
    public function getSuccessRate(): float;     // successCount / totalRows
    public function getErrors(): array;          // Array of BatchError
    public function count(): int;                // Returns totalRows
    public function getIterator(): \Traversable; // Iterate over errors
    public function getSummary(): string;        // "3 rows: 2 success, 1 errors"
}
```

### Firebird\BatchError

Per-row error value object with SQLSTATE classification.

```php
namespace Firebird;

class BatchError {
    public readonly int $position;     // Row position (0-based)
    public readonly string $sqlstate;  // SQLSTATE code (e.g., "23000")
    public readonly string $message;   // Error message
    public readonly int $errorCode;    // Firebird error code
    
    /**
     * Create from array data.
     */
    public static function fromArray(array $data): self;
    
    public function isConstraintViolation(): bool;  // SQLSTATE 23xxx
    public function isSyntaxError(): bool;          // SQLSTATE 42xxx
    public function getErrorClass(): string;        // First 2 chars of SQLSTATE
    public function __toString(): string;           // "Row 1: [23000] message"
}
```

### OO Wrapper Example

```php
<?php
use Firebird\Batch;
use Firebird\BatchResult;

$db = fbird_connect('localhost:/path/to/db.fdb', 'SYSDBA', 'masterkey');
$trans = fbird_trans($db);
$stmt = fbird_prepare($trans, 'INSERT INTO customers (id, name, email) VALUES (?, ?, ?)');

// Fluent API with method chaining
$result = Batch::fromQuery($stmt, $trans)
    ->add(1, 'Alice', 'alice@example.com')
    ->add(2, 'Bob', 'bob@example.com')
    ->add(3, 'Charlie', 'charlie@example.com')
    ->execute();

echo $result->getSummary() . "\n";  // "3 rows: 3 success, 0 errors"

if ($result->hasErrors()) {
    foreach ($result as $error) {
        echo "Error at row {$error->position}: {$error->message}\n";
    }
}

fbird_commit($trans);
fbird_close($db);
```

## Alternative: Multi-Row INSERT

For Firebird 3.x compatibility, consider multi-row INSERT syntax:

```sql
INSERT INTO table (col1, col2) VALUES 
    (?, ?), (?, ?), (?, ?), (?, ?)
```

Limitations:
- Parameter limit (max ~32k parameters)
- Variable row count requires dynamic SQL generation
- Less efficient than IBatch but wider compatibility

## See Also

- [docs/MLAZDANS_FIREBIRD_PHP_COMPARISON.md](MLAZDANS_FIREBIRD_PHP_COMPARISON.md)
- [Firebird 4.0 Release Notes - Batch Operations](https://firebirdsql.org/file/documentation/release_notes/html/en/4_0/rlsnotes40.html)
- [Firebird OO API Reference](https://firebirdsql.org/file/documentation/pdf/en/firebird-interfaces/interfaces.pdf)
