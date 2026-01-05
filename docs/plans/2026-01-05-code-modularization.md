# Code Modularization Plan: Despagettitize Large C Files

## Overview

Split the three largest C files into focused, maintainable modules to reduce complexity and improve code organization.

## Current State

| File | Lines | Domains |
|------|-------|---------|
| `firebird.c` | 3780 | 7 distinct domains mixed together |
| `fbird_query_exec.c` | 1693 | Query execution + statement management |
| `fbird_blobs.c` | 1246 | Blob handling + stream operations |

## Target Structure

### 1. Split firebird.c (3780 → 4 files)

**firebird.c (3780 lines)** contains:
- Module initialization (MINIT, MSHUTDOWN, GINIT)
- INI handling and displayers
- Error functions (errmsg, errcode, sqlstate)
- Connection management (connect, pconnect, close)
- Transaction management (trans, commit, rollback)
- Savepoint handling
- Batch operations
- Generator functions

#### Proposed Split:

**a. `fbird_core.c` (~700 lines) - Keep in firebird.c**
- Module init/shutdown (MINIT, MSHUTDOWN, RINIT, RSHUTDOWN, MINFO)
- GINIT/GSHUTDOWN
- INI registration and displayers
- Error handling: `_php_fbird_error()`, `fbird_errmsg()`, `fbird_errcode()`, `fbird_sqlstate()`
- Exception mode: `fbird_set_exception_mode()`, `fbird_get_exception_mode()`
- Client version functions
- `fbird_escape_string()`

**b. `fbird_connection.c` (~500 lines) - NEW**
- `_php_fbird_connect()` (internal)
- `fbird_connect()`
- `fbird_pconnect()`
- `fbird_close()`
- `fbird_drop_db()`
- `fbird_connection_info()`
- Link validation helpers
- Resource destructors for db_link

**c. `fbird_transaction.c` (~900 lines) - NEW**
- `fbird_trans_start()`
- `fbird_trans()` (main transaction function)
- `fbird_commit()`, `fbird_rollback()`
- `fbird_commit_ret()`, `fbird_rollback_ret()`
- `_php_fbird_trans_end()` (internal)
- Savepoint functions: `fbird_savepoint()`, `fbird_rollback_savepoint()`, `fbird_release_savepoint()`
- `fbird_trans_info()`
- `fbird_get_limbo_transactions()`
- `fbird_reconnect_transaction()`
- `fbird_gen_id()`
- Resource destructor for trans

**d. `fbird_batch.c` (~600 lines) - NEW**
- `fbird_batch_create()`
- `fbird_batch_add()`
- `fbird_batch_execute()`
- `fbird_batch_cancel()`
- `fbird_batch_add_blob()`
- `fbird_batch_register_blob()`
- Resource destructor for batch

### 2. Split fbird_query_exec.c (1693 → 2 files)

**Current structure:**
- ~1000 lines of internal helper functions
- `fbird_query()`, `fbird_prepare()`, `fbird_execute()`
- Multiple execute variants

#### Proposed Split:

**a. `fbird_query_exec.c` (~900 lines) - Reduce**
- `fbird_query()`
- `fbird_execute()`
- `fbird_execute_statement()`
- `fbird_execute_query()`
- `fbird_execute_auto()`
- `fbird_affected_rows()`
- Internal execute helpers

**b. `fbird_statement.c` (~800 lines) - NEW**
- `fbird_prepare()` (move from query_exec)
- `fbird_free_query()` (move from query_exec)
- Statement management internals
- Move statement-related helpers from `fbird_query_prepare.c`

### 3. Consolidate fbird_blobs.c (1246 - Already Well Organized)

**Current state:** Already focused on blob operations.

**Minor cleanup:**
- Already split into logical sections with comments
- Consider grouping all stream functions together
- No major refactoring needed

## Header File Organization

### New Headers Needed:

```
php_fbird_connection.h    - Connection function declarations
php_fbird_transaction.h   - Transaction function declarations
php_fbird_batch.h         - Batch function declarations
php_fbird_statement.h     - Statement function declarations
```

### Shared Header Updates:

`php_firebird.h` - Keep global declarations, include new headers

## Implementation Approach

### Phase 1: Extract Batch Operations (Low Risk)
1. Create `fbird_batch.c`
2. Move all `fbird_batch_*` functions
3. Create `php_fbird_batch.h`
4. Update Makefile/config.m4
5. Verify all batch tests pass

### Phase 2: Extract Transactions (Medium Risk)
1. Create `fbird_transaction.c`
2. Move transaction and savepoint functions
3. Create `php_fbird_transaction.h`
4. Update includes
5. Verify all transaction tests pass

### Phase 3: Extract Connections (Medium Risk)
1. Create `fbird_connection.c`
2. Move connect/close functions
3. Create `php_fbird_connection.h`
4. Update includes
5. Verify all connection tests pass

### Phase 4: Extract Statements (Low Risk)
1. Create `fbird_statement.c`
2. Move prepare/free functions
3. Create `php_fbird_statement.h`
4. Update includes
5. Verify all query tests pass

## Build Configuration Updates

### config.m4 Changes:

```m4
PHP_NEW_EXTENSION(fbird, 
    firebird.c 
    fbird_connection.c 
    fbird_transaction.c 
    fbird_batch.c 
    fbird_statement.c 
    fbird_query_exec.c 
    fbird_query_prepare.c 
    fbird_query_bind.c 
    fbird_query_array.c 
    fbird_result.c 
    fbird_blobs.c 
    fbird_events.c 
    fbird_service.c 
    fbird_metadata.c 
    fbird_inspection.c 
    fbird_datetime.c 
    firebird_utils.cpp, 
    $ext_shared,, $FBIRD_CFLAGS)
```

## Expected Outcome

### Before:
```
firebird.c           3780 lines (7 domains)
fbird_query_exec.c   1693 lines (2 domains)
fbird_blobs.c        1246 lines (1 domain)
```

### After:
```
firebird.c           ~700 lines (core/error handling)
fbird_connection.c   ~500 lines (connection domain)
fbird_transaction.c  ~900 lines (transaction domain)
fbird_batch.c        ~600 lines (batch domain)
fbird_query_exec.c   ~900 lines (query execution)
fbird_statement.c    ~800 lines (statement management)
fbird_blobs.c        1246 lines (unchanged, well-organized)
```

## Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| Static function visibility | Change `static` to internal linkage with `_php_` prefix |
| Cross-file dependencies | Document and minimize with clean interfaces |
| Build system complexity | Update config.m4 incrementally |
| Test failures | Run full test suite after each phase |

## Metrics for Success

- Each file < 1000 lines
- Single responsibility per file
- All tests pass after each phase
- No increase in binary size > 5%
- Clean compilation with -Wall -Werror

## Timeline Estimate

- Phase 1 (Batch): 2-3 hours
- Phase 2 (Transaction): 4-6 hours
- Phase 3 (Connection): 4-6 hours
- Phase 4 (Statement): 3-4 hours
- Testing & Integration: 2-3 hours

**Total: ~18-22 hours of focused work**

## Priority

**Recommend completing Issue #64 fix first** (NULL checks in fbird_service.c) since:
1. It's a bug fix with immediate value
2. Smaller scope, faster completion
3. Refactoring is enhancement, not bug fix
