# Implementation Plan

[Overview]
Implement comprehensive IBatch API enhancements including advanced BLOB handling, detailed per-row error reporting, and PHP OO wrapper classes.

This implementation extends the existing IBatch functionality in php-firebird to provide full-featured bulk operations support matching Firebird 4.0+ capabilities. The current implementation supports basic batch operations with pre-created BLOB IDs. This plan adds inline BLOB creation (addBlob, appendBlobData, addBlobStream), detailed per-row error status tracking, and a modern PHP OO wrapper layer (Firebird\Batch, BatchResult, BatchError classes).

The implementation follows Baby Steps™ methodology with TDD approach - each feature will have tests written first, then minimal implementation to pass tests, followed by refactoring. All changes maintain backward compatibility with existing procedural API.

[Types]
Define new C structures for batch BLOB handling and completion state tracking, plus PHP classes for OO interface.

### C Structures (firebird_utils.h)

```cpp
// Batch BLOB tracking structure
typedef struct {
    ISC_QUAD blob_id;       // Generated BLOB ID
    unsigned alignment;      // BLOB alignment from batch
    unsigned position;       // Position in batch message
} fbbatch_blob_entry;

// Extended completion state for detailed error reporting
typedef struct {
    unsigned position;       // Row position (0-based)
    int state;              // EXECUTE_FAILED or SUCCESS
    char* sqlstate;         // SQLSTATE code (5 chars + null)
    char* message;          // Error message (allocated)
} fbbatch_error_entry;

typedef struct {
    unsigned total_count;
    unsigned success_count;
    unsigned error_count;
    fbbatch_error_entry* errors;  // Array of error_count entries
} fbbatch_completion_result;
```

### PHP Structures (php_fbird_includes.h)

```c
// Extended fbird_batch structure
typedef struct {
    void *fbbatch_wrapper;    // OO API batch wrapper
    fbird_transaction *trans; // Associated transaction
    fbird_query *query;       // Parent prepared statement
    void *in_metadata;        // IMessageMetadata for input
    void *in_msg_buffer;      // Message buffer for row data
    unsigned in_msg_length;   // Message buffer size
    unsigned blob_count;      // Number of BLOBs added
    unsigned row_count;       // Number of rows added
} fbird_batch;
```

### PHP Classes (src/Firebird/)

```php
// Firebird\Batch - Main batch class
class Batch {
    private mixed $resource;
    private ?Transaction $transaction;
    private int $rowCount = 0;
    private array $blobIds = [];
}

// Firebird\BatchResult - Execution result
class BatchResult {
    public readonly int $totalRows;
    public readonly int $successCount;
    public readonly int $errorCount;
    private array $errors;
}

// Firebird\BatchError - Per-row error details
class BatchError {
    public readonly int $position;
    public readonly string $sqlstate;
    public readonly string $message;
    public readonly int $errorCode;
}
```

[Files]
Create new PHP classes, extend existing C files, add comprehensive tests.

### New Files to Create

| File | Purpose |
|------|---------|
| `src/Firebird/Batch.php` | OO wrapper for batch operations |
| `src/Firebird/BatchResult.php` | Batch execution result value object |
| `src/Firebird/BatchError.php` | Per-row error details value object |
| `tests/fbird_batch_blob_001.phpt` | Test inline BLOB handling |
| `tests/fbird_batch_blob_stream_001.phpt` | Test streaming BLOB handling |
| `tests/fbird_batch_errors_001.phpt` | Test detailed error reporting |
| `tests/fbird_batch_oo_001.phpt` | Test PHP OO wrapper |
| `tests/fbird_batch_multitype_001.phpt` | Comprehensive multi-type test |
| `tests/001-BATCH_TEST.sql` | Test table with various column types |

### Existing Files to Modify

| File | Changes |
|------|---------|
| `firebird_utils.h` | Add fbbatch_add_blob(), fbbatch_append_blob_data(), fbbatch_add_blob_stream(), fbbatch_register_blob(), fbbatch_set_default_bpb(), fbbatch_execute_detailed() declarations |
| `firebird_utils.cpp` | Implement new batch BLOB and error reporting functions |
| `php_fbird_includes.h` | Extend fbird_batch structure if needed |
| `firebird.c` | Add PHP functions: fbird_batch_add_blob(), fbird_batch_append_blob_data(), fbird_batch_execute() extended return, fbird_batch_get_errors() |
| `php_firebird.h` | Add function declarations and arginfo |
| `phpstan/fbird-functions.stub.php` | Add stubs for new functions |
| `docs/IBATCH_API_RESEARCH.md` | Update with implemented API documentation |
| `docs/FEATURE_TRANSFER_STATUS.md` | Mark features as complete |

[Functions]
New C wrapper functions and PHP userland functions for batch BLOB handling and error reporting.

### New C++ Wrapper Functions (firebird_utils.cpp)

```cpp
// Add inline BLOB data to batch
// Returns: BLOB position for binding, or -1 on error
int fbbatch_add_blob(
    void* master_ptr,
    void* batch_wrapper,
    unsigned length,
    const void* data,
    ISC_QUAD* blob_id_out,
    unsigned bpb_length,
    const unsigned char* bpb,
    ISC_STATUS* status_vector
);

// Append data to current BLOB (for chunked writes)
int fbbatch_append_blob_data(
    void* master_ptr,
    void* batch_wrapper,
    unsigned length,
    const void* data,
    ISC_STATUS* status_vector
);

// Stream-based BLOB addition
int fbbatch_add_blob_stream(
    void* master_ptr,
    void* batch_wrapper,
    unsigned length,
    const void* data,
    ISC_STATUS* status_vector
);

// Register existing BLOB for batch use
int fbbatch_register_blob(
    void* master_ptr,
    void* batch_wrapper,
    const ISC_QUAD* existing_blob,
    ISC_QUAD* batch_blob_id,
    ISC_STATUS* status_vector
);

// Set default BPB for BLOB operations
int fbbatch_set_default_bpb(
    void* master_ptr,
    void* batch_wrapper,
    unsigned bpb_length,
    const unsigned char* bpb,
    ISC_STATUS* status_vector
);

// Execute with detailed completion state
int fbbatch_execute_detailed(
    void* master_ptr,
    void* batch_wrapper,
    void* transaction_ptr,
    unsigned* total_processed,
    unsigned* success_count,
    unsigned* error_count,
    fbbatch_error_entry** errors,  // Allocated array, caller must free
    ISC_STATUS* status_vector
);

// Free error entries array
void fbbatch_free_errors(fbbatch_error_entry* errors, unsigned count);
```

### New PHP Functions (firebird.c)

```c
/* Create inline BLOB in batch context
 * Returns BLOB ID string for use in fbird_batch_add() */
PHP_FUNCTION(fbird_batch_add_blob);
// Signature: fbird_batch_add_blob(resource $batch, string $data [, int $type = 0]): string|false

/* Append data to current batch BLOB (for streaming large BLOBs) */
PHP_FUNCTION(fbird_batch_append_blob_data);
// Signature: fbird_batch_append_blob_data(resource $batch, string $data): bool

/* Stream BLOB data to batch */
PHP_FUNCTION(fbird_batch_add_blob_stream);
// Signature: fbird_batch_add_blob_stream(resource $batch, string $data): bool

/* Register existing BLOB for batch use */
PHP_FUNCTION(fbird_batch_register_blob);
// Signature: fbird_batch_register_blob(resource $batch, string $blob_id): string|false

/* Get detailed error information after execute */
PHP_FUNCTION(fbird_batch_get_errors);
// Signature: fbird_batch_get_errors(resource $batch): array

/* Get BLOB alignment requirement for batch */
PHP_FUNCTION(fbird_batch_get_blob_alignment);
// Signature: fbird_batch_get_blob_alignment(resource $batch): int|false
```

### Modified PHP Functions

```c
// fbird_batch_execute() - Extended return value
// Current: ['total_processed' => int, 'error_count' => int]
// New:     ['total_processed' => int, 'success_count' => int, 'error_count' => int, 'errors' => array]
// Where 'errors' contains per-row error details:
//   [['position' => int, 'sqlstate' => string, 'message' => string], ...]
```

[Classes]
PHP OO wrapper classes providing fluent interface for batch operations.

### Firebird\Batch (src/Firebird/Batch.php)

```php
namespace Firebird;

class Batch {
    private mixed $resource;
    private ?Transaction $transaction;
    private int $rowCount = 0;
    
    // Factory methods
    public static function create(mixed $query, ?Transaction $trans = null): self;
    public static function fromResource(mixed $resource): self;
    
    // Core operations
    public function add(mixed ...$params): self;
    public function execute(): BatchResult;
    public function cancel(): void;
    
    // BLOB operations
    public function addBlob(string $data, int $type = 0): BlobId;
    public function appendBlobData(string $data): self;
    public function addBlobStream(string $data): self;
    public function registerBlob(BlobId $blob): BlobId;
    public function setDefaultBpb(string $bpb): self;
    public function getBlobAlignment(): int;
    
    // Information
    public function getRowCount(): int;
    public function getResource(): mixed;
}
```

### Firebird\BatchResult (src/Firebird/BatchResult.php)

```php
namespace Firebird;

class BatchResult implements \Countable, \IteratorAggregate {
    public readonly int $totalRows;
    public readonly int $successCount;
    public readonly int $errorCount;
    private array $errors = [];
    
    // Factory
    public static function fromArray(array $data): self;
    
    // Status checks
    public function hasErrors(): bool;
    public function isComplete(): bool;
    public function getSuccessRate(): float;
    
    // Error access
    public function getErrors(): array;
    public function getErrorAt(int $position): ?BatchError;
    public function getFirstError(): ?BatchError;
    
    // Countable/IteratorAggregate
    public function count(): int;
    public function getIterator(): \Traversable;
    
    // Summary
    public function getSummary(): string;
}
```

### Firebird\BatchError (src/Firebird/BatchError.php)

```php
namespace Firebird;

class BatchError {
    public readonly int $position;
    public readonly string $sqlstate;
    public readonly string $message;
    public readonly int $errorCode;
    
    // Factory
    public static function fromArray(array $data): self;
    
    // Information
    public function isConstraintViolation(): bool;
    public function isSyntaxError(): bool;
    public function getErrorClass(): string;
    
    // String representation
    public function __toString(): string;
}
```

[Dependencies]
No new external dependencies required. Uses existing Firebird C API.

### Build Requirements
- Firebird 4.0+ client library (FB_API_VER >= 40)
- PHP 8.1+ (for readonly properties, constructor promotion)
- Existing project build configuration (unchanged)

### Conditional Compilation
All IBatch features are already wrapped in `#if FB_API_VER >= 40` blocks. New functions will follow the same pattern for Firebird 3.x compatibility (graceful degradation - functions return false/throw exceptions).

### PHPStan Stubs
Update `phpstan/fbird-functions.stub.php` with new function signatures for static analysis.

[Testing]
TDD approach with comprehensive test coverage for all new functionality.

### Test Files to Create

| Test File | Coverage |
|-----------|----------|
| `tests/fbird_batch_blob_001.phpt` | `fbird_batch_add_blob()` basic functionality |
| `tests/fbird_batch_blob_stream_001.phpt` | Streaming BLOB operations |
| `tests/fbird_batch_errors_001.phpt` | Detailed error reporting, multiple failure scenarios |
| `tests/fbird_batch_oo_001.phpt` | PHP OO wrapper (Batch, BatchResult, BatchError) |
| `tests/fbird_batch_multitype_001.phpt` | All SQL types in single batch |
| `tests/001-BATCH_TEST.sql` | Test table DDL for batch testing |

### Test Table Schema (001-BATCH_TEST.sql)

```sql
CREATE TABLE BATCH_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    INT_COL INTEGER,
    BIGINT_COL BIGINT,
    SMALLINT_COL SMALLINT,
    FLOAT_COL FLOAT,
    DOUBLE_COL DOUBLE PRECISION,
    NUMERIC_COL NUMERIC(18,4),
    DECIMAL_COL DECIMAL(10,2),
    CHAR_COL CHAR(20),
    VARCHAR_COL VARCHAR(100),
    DATE_COL DATE,
    TIME_COL TIME,
    TIMESTAMP_COL TIMESTAMP,
    BLOB_COL BLOB SUB_TYPE TEXT,
    BLOB_BIN_COL BLOB SUB_TYPE BINARY,
    BOOLEAN_COL BOOLEAN
);
```

### Existing Test Modifications

| Test File | Changes |
|-----------|---------|
| `tests/fbird_batch_001.phpt` | Add assertions for new return format |

### Coverage Requirements
- Minimum 80% code coverage for new functions
- All error paths tested
- NULL handling tested
- Edge cases (empty batches, max buffer) tested

[Implementation Order]
Sequential implementation following Baby Steps™ and TDD methodology.

### Phase 1: C++ Layer - BLOB Functions (4-6 hours)

1. **Add BLOB function declarations to firebird_utils.h**
   - `fbbatch_add_blob()`
   - `fbbatch_append_blob_data()`
   - `fbbatch_add_blob_stream()`
   - `fbbatch_register_blob()`
   - `fbbatch_set_default_bpb()`
   - `fbbatch_get_blob_alignment()` (declaration exists, verify implementation)

2. **Implement BLOB functions in firebird_utils.cpp**
   - Use IBatch::addBlob(), appendBlobData(), addBlobStream() from Firebird OO API
   - Handle BPB (Blob Parameter Block) for BLOB type specification
   - Proper error handling and status vector population

### Phase 2: C++ Layer - Error Reporting (2-3 hours)

3. **Add detailed error structures to firebird_utils.h**
   - `fbbatch_error_entry` structure
   - `fbbatch_completion_result` structure

4. **Implement fbbatch_execute_detailed() in firebird_utils.cpp**
   - Parse IBatchCompletionState for per-row status
   - Use completion->getState(), completion->findError(), completion->getStatus()
   - Allocate and populate error entries array
   - Add fbbatch_free_errors() for cleanup

### Phase 3: PHP Layer - BLOB Functions (3-4 hours)

5. **Write test first: tests/fbird_batch_blob_001.phpt**
   - Test inline BLOB creation
   - Test BLOB binding in batch add

6. **Add PHP function declarations to php_firebird.h**
   - PHP_FUNCTION declarations
   - ZEND_BEGIN_ARG_INFO_EX macros

7. **Implement PHP functions in firebird.c**
   - `fbird_batch_add_blob()` - Convert PHP string to BLOB, return BLOB ID
   - `fbird_batch_append_blob_data()` - Append to current BLOB
   - `fbird_batch_add_blob_stream()` - Stream BLOB data
   - `fbird_batch_register_blob()` - Register existing BLOB
   - `fbird_batch_get_blob_alignment()` - Return alignment value

8. **Register functions in module entry**
   - Add to zend_function_entry array

### Phase 4: PHP Layer - Error Reporting (2-3 hours)

9. **Write test first: tests/fbird_batch_errors_001.phpt**
   - Test error scenarios (constraint violations, type errors)
   - Verify per-row error details

10. **Modify fbird_batch_execute() return format**
    - Call fbbatch_execute_detailed() instead of fbbatch_execute()
    - Build extended return array with errors

11. **Implement fbird_batch_get_errors() function**
    - Return errors array from last execution

### Phase 5: PHP OO Wrapper (4-6 hours)

12. **Create src/Firebird/BatchError.php**
    - Implement value object with factory and helper methods

13. **Create src/Firebird/BatchResult.php**
    - Implement result container with Countable/IteratorAggregate

14. **Create src/Firebird/Batch.php**
    - Implement main batch class wrapping procedural functions
    - Fluent interface for add/execute/cancel operations
    - BLOB helper methods

15. **Write test: tests/fbird_batch_oo_001.phpt**
    - Test OO wrapper functionality

### Phase 6: Comprehensive Testing (2-3 hours)

16. **Create tests/001-BATCH_TEST.sql**
    - Test table with all supported column types

17. **Create tests/fbird_batch_multitype_001.phpt**
    - Insert rows with all column types
    - Test NULL handling
    - Test error scenarios

18. **Update documentation**
    - docs/IBATCH_API_RESEARCH.md - Mark features complete, update API docs
    - docs/FEATURE_TRANSFER_STATUS.md - Update status
    - phpstan/fbird-functions.stub.php - Add new function stubs

### Implementation Timeline
- **Total estimated time**: 17-25 hours
- **Critical path**: C++ BLOB functions → PHP BLOB functions → Tests
- **Parallel work possible**: OO wrapper can start after Phase 3

### Definition of Done (per phase)
- [ ] All tests pass
- [ ] PHPStan analysis passes
- [ ] Code follows project style (clang-tidy, PHP-CS-Fixer)
- [ ] Documentation updated
- [ ] No memory leaks (Valgrind check)
