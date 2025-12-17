# PHP Firebird Extension - Development History

This document summarizes the major development phases and milestones of the php-firebird extension modernization project.

## Project Timeline

| Phase | Date | Description |
|-------|------|-------------|
| **Initial Fork** | Nov 2025 | Forked from php/pecl-database-interbase |
| **Phase 1** | Nov 2025 | PHP 8.1+ requirement, build system modernization |
| **Phase 2** | Nov 2025 | Statement lifecycle analysis, PHP compatibility cleanup |
| **Phase 3** | Nov 2025 | C++17 modernization (RAII, std::optional, structured bindings) |
| **Phase 4** | Nov 2025 | CI/CD automation, cross-platform validation |
| **Phase 5** | Nov-Dec 2025 | Performance optimization research |
| **Extension Rename** | Dec 2025 | Complete rename from interbase→firebird, ibase_→fbird_ |
| **Event Timeout** | Dec 2025 | Event handling redesign with timeout and PHP wrapper classes |
| **OO API Phase 1-2** | Dec 2025 | Core infrastructure + Connection layer (C++ RAII wrappers) |
| **OO API Phase 3** | Dec 2025 | Connection OO API integration (`fbc_*` functions) |
| **OO API Phase 4** | Dec 2025 | Transaction OO API integration (`fbt_*` functions) |
| **OO API Phase 5** | Dec 2025 | Statement OO API integration (`fbs_*` functions) |
| **Issue #9 Fix** | Dec 2025 | Transaction cleanup segfault fix (use-after-free in DDL) |
| **Issue #10 Fix** | Dec 2025 | BLOB fetch segfault fix (zend_list_close() fix) |
| **TPB Comprehensive** | Dec 2025 | Full TPB support: all transaction flags, table reservation, Firebird 4.0+ features |
| **Issue #98 Fix** | Dec 2025 | Cross-platform date/time parsing (replaced strptime with portable sscanf) |

## Major Milestones

### C++17 Modernization (Phase 3)

**Functions Modernized:**
1. `fbu_get_client_version()` - std::optional safety + RAII
2. `fbu_encode_time()` - Input validation + constexpr optimization
3. `fbu_encode_date()` - Boundary checking + safe fallbacks
4. `fbu_decode_timestamp_tz()` - Structured bindings for 8-parameter function
5. `fbu_insert_field_info()` - Complete RAII metadata management
6. `fbu_insert_aliases()` - Modern iteration patterns

**C++17 Features Integrated:**
- RAII Resource Management (FirebirdMasterWrapper, FirebirdStatusManager)
- std::optional for safe error handling
- Structured bindings for multi-parameter functions
- constexpr compile-time optimization
- std::string_view for zero-copy string processing

### Extension Rename (December 2025)

**Breaking Changes Implemented:**
1. Extension module renamed: `interbase` → `firebird`
2. All PHP functions renamed: `ibase_*` → `fbird_*` (BC aliases retained)
3. All constants renamed: `IBASE_*` → `FBIRD_*` (no BC aliases - intentional break)
4. All source files renamed: `ibase_*.c` → `fbird_*.c`, `interbase.*` → `firebird.*`
5. All INI directives renamed: `ibase.*` → `fbird.*` (no BC aliases - clean break)

**INI Directives Renamed (14 total):**
- `fbird.allow_persistent` - Allow persistent connections
- `fbird.max_persistent` - Maximum persistent connections
- `fbird.max_links` - Maximum total connections
- `fbird.default_db` - Default database path
- `fbird.default_user` - Default username
- `fbird.default_password` - Default password
- `fbird.default_charset` - Default character set
- `fbird.timestampformat` - Timestamp format string
- `fbird.dateformat` - Date format string
- `fbird.timeformat` - Time format string
- `fbird.default_trans_params` - Default transaction parameters
- `fbird.default_lock_timeout` - Default lock timeout
- `fbird.enable_exceptions` - Enable exception mode
- `fbird.blob_segment_size` - BLOB segment size (NEW - default 4096)

**Files Renamed (51 total):**
- Source files: 9 C files renamed
- Test files: 41 .phpt files renamed
- Include files: php_interbase.h → php_firebird.h

### Event Timeout Implementation (December 2025)

**Problem Solved:**
The Firebird client library's `isc_wait_for_event()` blocks indefinitely and is not interruptible by signals on Linux. This made timeout-based event handling impossible at the C level.

**Solution - PHP Wrapper Classes:**
Implemented a strategy pattern with multiple polling approaches:

1. **ProcessEventPoller** - Process isolation via `proc_open()` (most reliable)
2. **PcntlEventPoller** - Signal-based timeout using SIGALRM (Unix only)
3. **FiberEventPoller** - Async integration with AMPHP (requires amphp/amp ^3.0)
4. **EventPoller** - Factory with auto-detection and strategy selection

**Files Added:**
- `src/Firebird/EventPollerInterface.php`
- `src/Firebird/EventPoller.php`
- `src/Firebird/ProcessEventPoller.php`
- `src/Firebird/PcntlEventPoller.php`
- `src/Firebird/FiberEventPoller.php`
- `tests/event_poller_wrapper.phpt`

**C Extension Changes:**
- `fbird_poll_event()` now accepts optional `int $timeout_ms` parameter
- Added `FBIRD_EVENT_TIMEOUT` constant (value: -2)
- Added `IBASE_EVENT_TIMEOUT` alias for compatibility

See [EVENT_TIMEOUT_RFC.md](development/EVENT_TIMEOUT_RFC.md) for full implementation details.

### Firebird OO API Modernization (December 2025)

**Objective:** Migrate from legacy `isc_*` C API to modern C++ Object-Oriented API for Firebird 3.0+.

**Completed Phases (1-5):**

| Phase | What | C Interop Functions |
|-------|------|---------------------|
| **Phase 1-2** | Core infrastructure: RAII wrappers, status handling, FB 4.0 compatibility | - |
| **Phase 3** | Connection layer: `IProvider::attachDatabase()` | `fbc_connect`, `fbc_disconnect`, `fbc_drop_database`, `fbc_is_connected`, `fbc_get_attachment`, `fbc_get_server_version` |
| **Phase 4** | Transaction layer: `ITransaction` lifecycle | `fbt_start`, `fbt_commit`, `fbt_rollback`, `fbt_commit_retaining`, `fbt_rollback_retaining`, `fbt_get_transaction` |
| **Phase 5** | Statement layer: `IStatement` + `IResultSet` cursor | `fbs_prepare`, `fbs_execute`, `fbs_open_cursor`, `fbs_fetch`, `fbs_close_cursor`, `fbs_free`, `fbs_is_cursor_open`, `fbs_get_affected_rows` |

**Key Technical Solutions:**

| Problem | Solution |
|---------|----------|
| FB 5.0-only `IStatus::hasData()` | Created `statusHasError()` helper using `getState() & STATE_ERRORS` |
| `IAttachment*` not interchangeable with `isc_db_handle` | Full cascading migration: connection → transaction → statement |
| Uninitialized OO API pointers causing segfaults | Explicit NULL initialization in all allocation paths |

**C++ RAII Wrapper Classes Created:**
- `ConnectionWrapper` (`src/cpp/fb_connection.hpp`) - Wraps `IAttachment`
- `TransactionWrapper` (`src/cpp/fb_transaction.hpp`) - Wraps `ITransaction`
- `StatementWrapper` (`src/cpp/fb_statement.hpp`) - Wraps `IStatement` + `IResultSet`
- `StatusWrapper` (`src/cpp/fb_status.hpp`) - Cross-version error handling
- `DpbBuilder` (`src/cpp/fb_dpb_builder.hpp`) - Database Parameter Block construction
- `TpbBuilder` (`src/cpp/fb_tpb_builder.hpp`) - Transaction Parameter Block construction

**Test Results:** 98 passed, 0 failed, 4 skipped (100% non-skipped pass rate)

See [MODERNIZATION_PLAN_FB3_TO_FB5.md](development/MODERNIZATION_PLAN_FB3_TO_FB5.md) for full implementation details.

### Issue #9: Transaction Cleanup Segfault Fix (December 2025)

**Problem Identified:**
When `fbird_drop_table_force()` committed a DDL transaction, the subsequent `fbird_close()` call caused a SIGSEGV (segmentation fault). 

**Root Cause Analysis:**
The `fbt_commit()` function in `firebird_utils.cpp` was calling `delete this` on the C++ TransactionWrapper after committing. However, the PHP resource system still held a reference to the deleted wrapper pointer, causing a use-after-free when `fbird_close()` attempted to clean up the transaction.

**Solution Implemented:**
1. **firebird_utils.cpp**: Modified `fbt_commit()` and `fbt_rollback()` to NOT delete the wrapper after operations - deferred to PHP resource destructor
2. **fbird_inspection.c**: Added explicit `fbt_free()` call in `_fbird_drop_table()` after `fbt_commit()` to prevent memory leaks

**Files Modified:**
- `firebird_utils.cpp` - Removed `delete this` from `fbt_commit()` and `fbt_rollback()`
- `fbird_inspection.c` - Added `fbt_free(trans_wrapper)` after DDL commit
- `tests/migration_001.phpt` - Removed `--XFAIL--` marker (test now passes)

**Test Validation:** All 104 tests pass, 0 failures

### Issue #10: BLOB Fetch Segfault After Commit (December 2025)

**Problem Identified:**
When fetching a large BLOB (65536 bytes) after the creating transaction had been committed, a SIGSEGV occurred during BLOB read operations in `fbird_fetch_assoc()`.

**Root Cause Analysis:**
The `fbird_blob_close()` function used `zend_list_delete()` to destroy the BlobWrapper resource. However, `zend_list_delete()` immediately frees the resource regardless of reference count, causing a use-after-free when subsequent operations tried to access the BLOB data.

**Solution Implemented:**
Replaced `zend_list_delete()` with `zend_list_close()` in `fbird_blobs.c`. The `zend_list_close()` function properly decrements the reference count and only frees the resource when no references remain, allowing the PHP resource destructor to handle cleanup at the correct time.

**Files Modified:**
- `fbird_blobs.c` - Changed `zend_list_delete()` to `zend_list_close()` in `PHP_FUNCTION(fbird_blob_close)`
- `tests/blob_stream_chunked_write.phpt` - Removed `--XFAIL--` marker (test now passes)

**Test Validation:** All 105 tests pass, 0 failures, 0 expected failures


### Issue #98: Cross-Platform Date/Time Format Parsing (December 2025)

**Problem Identified:**
The extension used `strptime()` for parsing date/time strings when binding parameters. This function is POSIX-specific and not available on Windows, has different behavior on macOS and musl libc (Alpine Linux), making the extension non-portable.

**Root Cause Analysis:**
- `strptime()` is not part of ISO C, only POSIX
- Windows has no native `strptime()` implementation
- macOS/musl libc implementations differ from glibc
- The `_GNU_SOURCE` define was required for Linux builds

**Solution Implemented:**
Created a dedicated cross-platform date/time parsing module (`fbird_datetime.c`/`.h`) using portable `sscanf()`-based parsing with automatic format detection:

**Date Formats Supported (auto-detected):**
- ISO 8601: `YYYY-MM-DD` (e.g., `2025-12-17`)
- European: `DD.MM.YYYY` (e.g., `17.12.2025`)
- US: `MM/DD/YYYY` (e.g., `12/17/2025`)

**Time Formats Supported:**
- With fractional seconds: `HH:MM:SS.ssss` (up to microseconds)
- Without fractional: `HH:MM:SS`
- With timezone: `HH:MM:SS+HH:MM` or `HH:MM:SS America/New_York`

**Timezone Support (Firebird 4.0+):**
- Offset format: `+HH:MM`, `-HH:MM`, `+HHMM`, `-HHMM`
- Named zones: `America/New_York`, `Europe/Berlin`, etc.
- UTC shorthand: `Z`

**API Functions Created:**
- `fbird_parse_date()` - Parse date string to components
- `fbird_parse_time()` - Parse time string to components
- `fbird_parse_timestamp()` - Parse combined timestamp
- `fbird_validate_date()` - Validate date (leap years, days in month)
- `fbird_validate_time()` - Validate time ranges
- `fbird_extract_timezone()` - Extract timezone from time string

**Files Added:**
- `fbird_datetime.h` - Header with `fbird_datetime_components` structure and API
- `fbird_datetime.c` - Portable implementation (~350 lines)

**Files Modified:**
- `fbird_query_bind.c` - Replaced `strptime()` with `fbird_parse_*()` functions
- `fbird_query_array.c` - Replaced `strptime()` with `fbird_parse_*()` functions, removed `_GNU_SOURCE`
- `config.m4` - Added `fbird_datetime.c` to build sources

**Test Validation:**
All 5 date/time tests pass:
- `time_003.phpt` - Date parameter binding
- `time_004.phpt` - Time parameter binding
- `timezone_001.phpt` - TIMESTAMP WITH TIME ZONE
- `timezone_002.phpt` - TIME WITH TIME ZONE
- `timezone_003.phpt` - Timezone operations

**Platform Compatibility:**
- Linux (glibc): ✅ Tested
- Linux (musl/Alpine): ✅ Portable sscanf()
- Windows: ✅ No POSIX dependency
- macOS: ✅ No platform-specific behavior

## Key Architectural Decisions

### PHP Version Support
- **Minimum:** PHP 8.1 (enforced via configure-time check)
- **Tested:** PHP 8.1, 8.2, 8.3, 8.4, 8.5

### Firebird Version Support
- **Client Library Requirement:** Firebird 3.0+ client library (uses OO API)
- **Server Connectivity:** FB 3.0 client → FB 2.5-3.0, FB 4.0 client → FB 2.5-4.0, FB 5.0 client → FB 2.5-5.0+
- **Conditional compilation:** FB_API_VER >= 30/40 patterns for version-specific features
- **Advanced features:** Timezone support (Firebird 4.0+ client and server)

### Breaking Change Policy
BC intentionally not maintained for constants to clearly signal the new driver without InterBase roots. Function aliases provide migration path for existing code.

## Performance Validation

**C++17 Feature Benchmarks:**
- std::optional operations: ~84 ns/call (minimal overhead)
- Input validation: ~1.9% overhead (near-zero due to constexpr)
- Move semantics: 25.3% faster than copy operations
- Zero performance regression confirmed

## Quality Infrastructure

**Static Analysis:**
- clang-tidy: C++17 modernization checks
- Cppcheck: Static analysis for undefined behavior
- AddressSanitizer: Memory error detection
- Valgrind: Memory leak detection

**CI/CD:**
- GitLab CI: 7-stage pipeline with quality gates
- GitHub Actions: Cross-platform validation (Windows, macOS, Linux)
- Docker: Multi-PHP version testing (8.1-8.5)

### TPB Comprehensive Support (December 2025)

**Objective:** Provide full Transaction Parameter Block (TPB) support for all Firebird transaction features.

**Features Implemented and Verified:**
- **Access Modes**: `FBIRD_READ`, `FBIRD_WRITE`
- **Isolation Levels**: `FBIRD_CONCURRENCY` (SNAPSHOT), `FBIRD_COMMITTED` (READ COMMITTED), `FBIRD_CONSISTENCY` (SERIALIZABLE)
- **Record Versioning**: `FBIRD_REC_VERSION`, `FBIRD_REC_NO_VERSION`
- **Lock Resolution**: `FBIRD_WAIT`, `FBIRD_NOWAIT`, `FBIRD_LOCK_TIMEOUT`
- **Table Reservation**: `FBIRD_LOCK_SHARED`, `FBIRD_LOCK_PROTECTED`, `FBIRD_LOCK_EXCLUSIVE`, `FBIRD_LOCK_READ`, `FBIRD_LOCK_WRITE`
- **Firebird 4.0+**: `FBIRD_READ_CONSISTENCY` (read consistency for READ COMMITTED isolation)

**Bug Fixed:**
The `fbxpb_build_tpb()` function was manually inserting `isc_tpb_version3` but the IXpbBuilder OO API already adds it automatically. This caused duplicate version bytes in TPB (e.g., `03 03 09 02` instead of `03 09 02`), leading to transaction failures or unexpected behavior.

**API Patterns:**
1. **Flag-first API**: `fbird_trans(FLAGS, $db)` - Simple, combinable flags with `|`
2. **Array options API**: `fbird_trans_start($db, $options)` - Full TPB control including table reservation
3. **Transaction inspection**: `fbird_trans_info($trans)` - Returns transaction ID, state, isolation level

**Test Coverage:**
- `tests/trans_tpb_comprehensive.phpt` - 16 transaction tests covering all TPB features
- All tests pass with 100% success rate

**Files Modified:**
- `firebird_utils.cpp` - Fixed `fbxpb_build_tpb()` to not manually insert version byte
- `firebird.c` - Transaction API functions
- `php_fbird_includes.h` - Transaction flag constants

## Test Coverage

- **Total tests:** 110 PHPT test files
- **Pass rate:** 100% (all non-skipped tests pass)
- **Coverage areas:** Connection, transactions (comprehensive TPB), queries, blobs, services, metadata, inspection, savepoints

---

*This document consolidates historical development reports from the modernization project.*
*For current development guides, see `docker.md`, `local_qa_workflow.md`, and `BRANCHING.md`.*
