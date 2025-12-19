# IBatch API Research

## Overview

The IBatch interface in Firebird 4.0+ provides high-performance bulk operations for inserting large volumes of data. This document researches the feasibility of implementing IBatch support in php-firebird.

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
