# Implementation Plan: Issue #56 Heap Corruption Fix

**Date**: 2026-01-03
**Issue**: https://github.com/satwareAG/php-firebird/issues/56
**Status**: Investigation Complete, Implementation Required

## [Overview]

Fix heap corruption causing SIGSEGV/SIGABRT during PHP shutdown in doctrine-firebird-driver test suite.

The doctrine-firebird-driver team has provided critical debugging findings that point to a Zend Memory Manager interaction issue. The key discovery is that `USE_ZEND_ALLOC=0` (bypassing PHP's Zend MM) eliminates the crash entirely, proving the corruption occurs within the extension's interaction with PHP's memory management.

## [Evidence Summary]

| Test Condition | Exit Code | Result |
|----------------|-----------|--------|
| Normal run | 139 (SIGSEGV) or 134 (SIGABRT) | Crash |
| `USE_ZEND_ALLOC=0` | 0 | No crash |

### GDB Analysis
- RDI register contains ASCII "uildrer" (part of "Builder") instead of valid pointer
- Crash occurs in `firebird.so` during `zend_shutdown_executor_values`
- `si_code=SI_KERNEL`, `si_addr=NULL` - NULL pointer dereference after heap corruption

### Root Cause Hypothesis
Memory allocator mismatch or use-after-free:
1. Extension frees memory via `efree()` or `delete`
2. Zend MM reuses memory for PHP objects ("QueryBuilder" string)
3. Stale pointer dereferences the reused memory → crash

## [Types]

No new types required. Existing types to audit:

```c
// fbird_service in fbird_service.c
typedef struct {
    void *handle;
    char *hostname;
    char *username;
    zend_resource *res;
    void *fbsvc_service;     // OO API ServiceWrapper*
    pid_t created_pid;       // Fork detection
} fbird_service;

// fbird_db_link in php_firebird.h
typedef struct {
    union { void* ptr; ISC_INT64 val; } handle;
    char *username;
    void *fbc_connection;    // OO API Connection*
    void *fbc_transaction;   // OO API Transaction*
    zend_resource *res;
    zend_resource *default_trans_res;
    char hash_key[128];
    pid_t created_pid;       // Fork detection
} fbird_db_link;
```

## [Files]

Files to modify:
- `firebird.c` - Add debug hooks, audit destructors
- `fbird_service.c` - Add MSHUTDOWN safety to service destructor
- `firebird_utils.cpp` - Add NULL guards after free patterns
- `php_firebird.h` - Add debug macros

Files to create:
- `docs/plans/2026-01-03-issue-56-asan-findings.md` - ASAN output analysis

## [Functions]

### Critical Destructors to Audit

| File | Function | Purpose |
|------|----------|---------|
| `firebird.c` | `_php_fbird_close_link()` | Connection destructor |
| `firebird.c` | `_php_fbird_close_plink()` | Persistent connection destructor |
| `firebird.c` | `_php_fbird_free_trans()` | Transaction destructor |
| `firebird.c` | `_php_fbird_free_batch()` | Batch destructor |
| `fbird_service.c` | `_php_fbird_free_service()` | Service destructor |
| `fbird_events.c` | `_php_fbird_free_event()` | Event destructor |

### OO API Cleanup Functions

| File | Function | Issue |
|------|----------|-------|
| `firebird_utils.cpp` | `fbc_disconnect()` | Deletes Connection, must NULL after |
| `firebird_utils.cpp` | `fbt_free()` | Deletes Transaction |
| `firebird_utils.cpp` | `fbs_free()` | Deletes Statement |
| `fb_service.hpp` | `fbsvc_detach()` | Detaches service |
| `fb_service.hpp` | `fbsvc_free()` | Deletes ServiceWrapper |

## [Classes]

C++ classes to audit:

- `fb::Connection` - Connection wrapper, owns IAttachment*
- `fb::Transaction` - Transaction wrapper, owns ITransaction*
- `fb::StatementWrapper` - Statement wrapper, owns IStatement*
- `fb::ServiceWrapper` - Service wrapper, owns IService*

Key concern: These classes hold raw Firebird interface pointers. If the underlying
interface is detached/released, the wrapper still holds a dangling pointer until
explicitly freed.

## [Dependencies]

Build dependencies for ASAN:
```bash
# Required flags for ASAN build
CFLAGS="-fsanitize=address -fno-omit-frame-pointer -g -O1"
CXXFLAGS="-fsanitize=address -fno-omit-frame-pointer -g -O1"
LDFLAGS="-fsanitize=address"
```

No new runtime dependencies.

## [Testing]

### ASAN Build Procedure

```bash
# Clean build environment
make clean
phpize --clean

# Configure with ASAN
phpize
./configure CFLAGS="-fsanitize=address -fno-omit-frame-pointer -g -O1" \
            CXXFLAGS="-fsanitize=address -fno-omit-frame-pointer -g -O1" \
            LDFLAGS="-fsanitize=address"

# Build
make -j$(nproc)

# Run tests with ASAN
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 make test
```

### Reproduction Test

```php
<?php
// Minimal reproduction case from doctrine-firebird-driver
$connection = fbird_connect('firebird:3050/test.fdb', 'SYSDBA', 'masterkey');
$result = fbird_query($connection, 'SELECT 1 FROM RDB$DATABASE');
fbird_fetch_assoc($result);
fbird_free_result($result);
fbird_close($connection);
// Crash happens during PHP shutdown, not here
```

## [Implementation Order]

1. Build extension with ASAN flags
2. Run doctrine-firebird-driver test suite to trigger crash
3. Analyze ASAN output for exact corruption location
4. Implement fix based on ASAN findings
5. Verify fix with:
   - ASAN clean run
   - Valgrind clean run
   - Normal test suite (135/138 pass)
6. Update Issue #56 with findings and fix
7. Create release candidate v7.0.0-rc.43

## [ASAN Build Commands]

```bash
# In php-firebird directory
cd /home/mw/CLionProjects/php-firebird

# Clean
make clean 2>/dev/null; phpize --clean 2>/dev/null

# Configure with ASAN
phpize
./configure \
  CFLAGS="-fsanitize=address -fno-omit-frame-pointer -g -O1" \
  CXXFLAGS="-fsanitize=address -fno-omit-frame-pointer -g -O1" \
  LDFLAGS="-fsanitize=address"

# Build
make -j$(nproc)

# Run specific test
ASAN_OPTIONS=detect_leaks=0:abort_on_error=0:print_scariness=1 \
  php -d extension=./modules/firebird.so test_script.php
```

## [References]

- Issue #56: https://github.com/satwareAG/php-firebird/issues/56
- doctrine-firebird-driver debugging analysis in issue comment
- Previous fork-safety fixes: Issue #22, #35, #36, #55
