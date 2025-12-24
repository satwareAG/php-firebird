# Changelog

All notable changes to the PHP Firebird Extension will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [7.0.0-rc.7] - 2025-12-24

### Added

- **Script Infrastructure Update**:
  - Consolidated all scripts into `scripts/` directory (removed `host/` vs `container/` split)
  - Updated all scripts (`qa.sh`, `test.sh`, `test_matrix.sh`) to use `set -euo pipefail` for robustness
  - Modernized `qa.sh` (formerly qa_local.sh) as primary local quality gateway
  - Updated Docker files and CI workflows to reflect new script locations

### Fixed

- **PSR-12 Code Style Compliance (dbcf95e)**: Fixed file-level docblock positioning in 13 OO wrapper classes
  - File-level docblocks moved BEFORE `declare(strict_types=1)` per PSR-12 standard
  - Affected files: `src/Firebird/*.php` (Batch, BatchError, BatchResult, BlobId, Database, DbInfo, Event, EventPoller, Exception, Query, TBuilder, Transaction, functions.php)
  - Resolved 13 PHPCS violations: "File comment must be between the open tag and the declare statement"
  - Verification: `vendor/bin/phpcs src/` now returns 0 errors (1 acceptable warning)
  - All PHPStan Level 8 checks remain passing
  - All smoke tests passing (Batch, BlobId, Transaction)

- **GitHub Actions CI/CD (e338455)**: Fixed missing SKIPIF logic in `tests/issue23_alias_padding_001.phpt`
  - Test had comment documenting skip requirement for non-4.0 versions but skip logic was not implemented
  - Added skip condition: `if ($fb_version < 4.0 || $fb_version >= 5.0) die(...)`
  - Test now correctly skips on Firebird 2.5, 3.0, and 5.0 (runs only on 4.x)
  - Resolves GitHub Actions test matrix failures across all non-4.0 Firebird versions

- **Quality Assurance**: 
  - Comprehensive QA completed: PHPStan Level 8 (0 errors), PHPCS PSR-12 (0 violations), all tests passing
  - GitHub Issue #23 (Column alias deduplication) verified as fixed in rc.6 and closed
  - QA Summary documented in `QA_SUMMARY.md`
  - Standardized shell script error handling and variable usage
  - ShellCheck basic validations applied to infrastructure scripts
  - Removed duplicated functionality between host/container scripts

## [7.0.0-rc.6] - 2025-12-24

### Added

- **Exception Mode API (#15)**: PDO-style exception handling for clean error management
  - `fbird_set_exception_mode(int $mode): bool` - Set runtime exception mode (SILENT or THROW)
  - `fbird_get_exception_mode(): int` - Get current exception mode
  - Constants: `FBIRD_EXCEPTION_MODE_SILENT` (0, default), `FBIRD_EXCEPTION_MODE_THROW` (1)
  - `Firebird\Exception::getSqlState(): string` - Return SQLSTATE error code (e.g., "23000", "42000")
  - Runtime mode takes precedence over INI setting `fbird.enable_exceptions`
  - Required for Doctrine DBAL integration (PDO::ERRMODE_EXCEPTION compatibility)
  - Backward compatible: SILENT mode is default, maintains existing behavior
  - Tests: `tests/fbird_exception_mode_001.phpt`, `tests/fbird_exception_mode_002.phpt`

### Fixed

- **Issue #22 (Fork-safety)**: Extension no longer segfaults when loaded in forked child processes (pcntl_fork, PHPStan parallel, PHPUnit parallel)
  - Added PID tracking to detect forked processes
  - Resource destructors skip Firebird API cleanup in forked children
  - Only parent process performs connection/transaction/batch cleanup
  - Test: `tests/issue22_pcntl_fork_001.phpt`
  - Impact: Enables parallel processing tools (PHPStan, PHPUnit, Infection, Psalm, custom worker pools)

- **Issue #23 (Column alias padding)**: Column alias deduplication now works correctly with space-padded aliases (Firebird 3.0+)
  - Firebird 3.0+ returns CHAR-type column aliases padded with trailing spaces to declared length
  - Added `_php_fbird_rtrim_alias()` helper to trim trailing whitespace before alias registration
  - Prevents duplicate array keys in `fbird_fetch_assoc()` when aliases differ only by padding
  - Test: `tests/issue23_alias_padding_001.phpt`
  - Impact: Fixes associative array key collisions when using CHAR-type column aliases

## [7.0.0-rc.5] - 2025-12-24

### Fixed

- **CI Extension Loading**: Use `PHP_TEST_SHARED_EXTENSIONS` environment variable for PHPT test runner instead of `-d extension=` argument (commit f109ef9)
- **PHPStan Configuration**: 
  - Remove duplicate function stub file (`firebird.stub.php` vs `fbird.stub.php`)
  - Correct stub file syntax for PHP 8.1+ compatibility
  - Change `list<mixed>` to `array<int, mixed>` in function docblocks for stricter type checking
- **PHPCS Configuration**: Exclude `PSR1.Files.SideEffects` rule to allow `src/Firebird/functions.php` with define() + function definitions
- **Test SKIPIF Sections**: 
  - Add Firebird version compatibility conditions to tests using features not available in older versions
  - `tests/003.phpt`: Skip on Firebird < 4.0 (INT128/DECFLOAT types)
  - `tests/fbird_inspection_001.phpt`: Skip on Firebird < 3.0 (MON$ATTACHMENTS columns)
  - Fix duplicate `firebird.inc` include in SKIPIF sections causing "Cannot redeclare" errors

### Added

- **Local CI Testing Guide**: `docs/development/LOCAL_CI_TESTING.md` documenting `act` tool usage for testing GitHub Actions locally

## [7.0.0-rc.4] - 2025-12-23

### Fixed

- **DbInfo::fromConnection()**: Implemented DbInfo hydration via `fbird_connection_info()` (removes stale TODO; previously always returned empty info).

### Changed

- **QA/Static Analysis**: Follow-up cleanups to keep clang-tidy/cppcheck pipelines green (post-rc.3 commits).

## [7.0.0-rc.3] - 2025-12-22

### Fixed

- **Issue #21 (TIME encoding corruption)**: Ensure TIME parsing always initializes fields to avoid uninitialized-memory corruption.

## [7.0.0-rc.2] - 2025-12-22

### Fixed

- **Issue #19 (Firebird 3.0 compilation compatibility)**: Added `fb_blr_compat.h` fallback definitions for missing `firebird/impl/blr.h` (FB 4.0+ only).
- **Firebird 3.0 runtime compatibility**: Use `hasData()` instead of `isDirty()` for FB3 compatibility.

### Changed

- **CI matrix**: GitHub Actions now uses matching Firebird client version for each server version.

## [7.0.0-rc.1] - 2025-12-21

### Fixed

- **Transaction list cleanup bug**: Fixed memory management for default transaction slot in `fbird_db_link->tr_list`. The first node (default transaction) should not be freed during cleanup, only cleared. This prevented potential use-after-free issues when using implicit transactions. (commit 477ecea)
- **Migration test DDL commits**: Fixed `tests/migration_001.phpt` segmentation fault by adding explicit `fbird_commit()` after each `CREATE TABLE` statement. Firebird requires DDL commits before DML can reference newly created tables. (commit 2f0718f)

### Added

- **IBatch API (Firebird 4.0+)**: High-performance bulk operations for 10-12x INSERT speedup
  - **Procedural Functions:**
    - `fbird_batch_create($query [, $trans])` - Create batch from prepared statement
    - `fbird_batch_add($batch, ...$params)` - Add row with automatic type conversion
    - `fbird_batch_add_blob($batch, $data [, $type])` - Create inline BLOB, returns "HHHHHHHH:LLLL" ID
    - `fbird_batch_register_blob($batch, $blob_id)` - Register existing BLOB for batch use
    - `fbird_batch_execute($batch)` - Execute batch, returns `['total_processed', 'success_count', 'error_count']`
    - `fbird_batch_cancel($batch)` - Cancel without executing
  - **OO Wrapper Classes:**
    - `Firebird\Batch` - Main batch class with fluent `fromQuery()`, `add()`, `execute()` methods
    - `Firebird\BatchResult` - Result container implementing `Countable`, `IteratorAggregate`
    - `Firebird\BatchError` - Per-row error value object with SQLSTATE classification
  - **BLOB ID Format:** Standardized "HHHHHHHH:LLLL" (13 characters, colon-separated hex)
  - **Supported SQL Types:** INTEGER, BIGINT, SMALLINT, FLOAT, DOUBLE PRECISION, NUMERIC, DECIMAL, CHAR, VARCHAR, DATE, TIME, TIMESTAMP, TIME WITH TIME ZONE, TIMESTAMP WITH TIME ZONE, BOOLEAN, BLOB (TEXT/BINARY)
  - **NULL Handling:** Full NULL support for all column types
  - **Error Behavior:** IBatch stops processing on first error by default; rows before error committed
  - **Tests (6 total):**
    - `tests/blobid_001.phpt` - BlobId value object
    - `tests/fbird_batch_001.phpt` - Basic batch operations
    - `tests/fbird_batch_blob_001.phpt` - BLOB operations (add_blob, register_blob)
    - `tests/fbird_batch_errors_001.phpt` - Error reporting and success_count
    - `tests/fbird_batch_multitype_001.phpt` - Comprehensive multi-type with NULL handling
    - `tests/fbird_batch_oo_001.phpt` - OO wrapper classes
- **Limbo Transaction Recovery Functions**: For handling failed two-phase commits
  - `fbird_get_limbo_transactions([resource $link [, int $max_count]])` - Retrieve list of in-doubt transaction IDs
  - `fbird_reconnect_transaction(resource $link, int $transaction_id)` - Reconnect to limbo transaction for manual commit/rollback
  - Validates max_count parameter (1-10000 range)
  - Returns transaction resource compatible with `fbird_commit()` and `fbird_rollback()`
  - Test: `tests/fbird_limbo_trans_001.phpt`
- **`fbird_sqlstate()` function**: Returns 5-character SQLSTATE error code (SQL:2003 standard) for better error classification
  - Returns `"23000"` for integrity constraint violations
  - Returns `"42000"` for syntax errors or access rule violations
  - Returns `"HY000"` for general errors
  - Returns `false` if no error has occurred
  - Uses Firebird's `fb_sqlstate()` API (Firebird 2.5+)
- **`fbird_connection_info()` function**: Returns database connection statistics and information
  - Performance metrics: reads, writes, fetches, marks
  - Configuration: page_size, num_buffers, sql_dialect
  - Memory stats: current_memory, max_memory, allocation
  - Identifiers: attachment_id, ods_version, ods_minor_version
- **`fbird_blob_seek()` function**: Seek within stream BLOBs for random access
  - Constants: `FBIRD_BLOB_SEEK_SET`, `FBIRD_BLOB_SEEK_CUR`, `FBIRD_BLOB_SEEK_END`
- **`FBIRD_FETCH_DATE_OBJ` constant**: Return DATE/TIME/TIMESTAMP columns as DateTimeImmutable objects
  - Use with `fbird_fetch_assoc()`, `fbird_fetch_row()`, `fbird_fetch_object()`
  - Example: `$row = fbird_fetch_assoc($result, FBIRD_FETCH_DATE_OBJ);`
  - Can be combined with other flags (e.g., `FBIRD_FETCH_BLOBS | FBIRD_FETCH_DATE_OBJ`)
- **PHP OO Wrappers**: High-level PHP classes for modern development
  - `Firebird\Database` - Connection management with query helpers
  - `Firebird\Transaction` - Transaction handling with savepoint support
  - `Firebird\TBuilder` - Fluent transaction parameter builder
  - `Firebird\BlobId` - Type-safe BLOB identifier value object
  - `Firebird\DbInfo` - Database information structure

### Changed

- **SPDX License Headers**: Migrated 37 source files from verbose 10-24 line PHP extension headers to minimal 2-line SPDX-compliant headers (~85% reduction in header boilerplate)
- **CREDITS File**: Comprehensive attribution with GitHub contributor links
- **Documentation Standards**: Added `docs/DOCUMENTATION_STANDARDS.md` defining project documentation conventions

## [1.0.0] - 2025-12-17

### ⚠️ Breaking Changes

- **Extension renamed from `interbase` to `firebird`**
  - Build output: `firebird.so` (not `interbase.so`)
  - PHP configuration: `extension=firebird.so`
  - No backward compatibility aliases for extension name

- **Functions renamed from `ibase_*` to `fbird_*`**
  - Example: `ibase_connect()` → `fbird_connect()`
  - BC aliases retained: `ibase_*()` functions still work but are deprecated

- **Constants renamed from `IBASE_*` to `FBIRD_*`**
  - Example: `IBASE_READ` → `FBIRD_READ`
  - No BC aliases for constants (intentional clean break)

- **INI directives renamed from `ibase.*` to `fbird.*`**
  - Example: `ibase.default_user` → `fbird.default_user`
  - All 14 INI directives renamed

### Added

- **Modern C++ OO API**: Uses Firebird 3.0+ Object-Oriented API with RAII wrappers
- **PHP 8.1+ Support**: Optimized for PHP 8.1, 8.2, 8.3, 8.4, and 8.5
- **Firebird 5.0 Support**: Full compatibility with Firebird 2.5, 3.0, 4.0, and 5.0
- **Thread-Safe Event Handling**: Complete redesign using polling model (`fbird_poll_event()`)
- **`FBIRD_CONNECT_FORCE_NEW` flag**: Explicit new connection creation (matches PostgreSQL pattern)
- **Cross-platform date/time parsing**: `sscanf()`-based parsing replacing non-portable `strptime()`
- **Comprehensive Test Suite**: 113 PHPT tests covering all functionality
- **CI/CD Pipeline**: GitHub Actions with multi-version PHP/Firebird matrix testing
- **Docker Development Environment**: Multi-version Dockerfiles for PHP 8.1-8.5
- **Static Analysis**: Cppcheck, clang-tidy, AddressSanitizer integration
- **Code Coverage**: Linux code coverage workflow

### Fixed

- **Issue #99**: CHAR fields now correctly report type as "CHAR" (not "VARCHAR")
- **Issue #98**: Date/time parsing now cross-platform (no `strptime()` dependency)
- **Issue #97**: Connection reuse documented + `FBIRD_CONNECT_FORCE_NEW` flag added
- **Issue #82**: Complete migration from IBASE to FBIRD naming
- **Issue #71**: `fbird_service_attach()` now respects INI defaults (`fbird.default_user`/`fbird.default_password`)
- **Issue #66**: Event handling PHP 8.4+ stack overflow fixed via polling model
- **Issue #53**: Service attach local connection works without TCP prefix
- **Issue #45**: Event test memory leak fixed via single-threaded execution
- **Issue #42**: Array handling tests (007.phpt) re-enabled and passing
- **Issue #25**: CHAR(1) UTF-8 padding issue fixed (proper trimming)
- **Issue #22**: `fbird_close()` now returns `false` on second call (correct behavior)

### Changed

- **Minimum PHP version**: 8.1 (up from 7.x)
- **Minimum Firebird client**: 3.0+ (uses OO API)
- **Build system**: Updated autoconf/automake configuration
- **Test structure**: Reorganized into 5-layer pyramid architecture

### Deprecated

- **`ibase_*()` function aliases**: Use `fbird_*()` functions instead
- These aliases will be removed in version 2.0.0

### Security

- Memory safety improvements with AddressSanitizer validation
- Thread-safety fixes in event handling
- Resource lifecycle management improvements

## Migration Guide

### From Legacy `interbase` Extension

1. **Update php.ini**:
   ```ini
   # Old (remove)
   extension=interbase.so
   ibase.default_user=SYSDBA
   
   # New (add)
   extension=firebird.so
   fbird.default_user=SYSDBA
   ```

2. **Update function calls**:
   ```php
   // Old (deprecated)
   $conn = ibase_connect($database, $user, $password);
   
   // New (recommended)
   $conn = fbird_connect($database, $user, $password);
   ```

3. **Update constants**:
   ```php
   // Old (will not work)
   $trans = fbird_trans(IBASE_READ, $conn);
   
   // New (required)
   $trans = fbird_trans(FBIRD_READ, $conn);
   ```

### Testing Migration

```bash
# Check for legacy function usage
grep -r "ibase_" src/

# Check for legacy constants
grep -rE "IBASE_[A-Z]+" src/

# Check INI directives
grep -r "ibase\." config/
```

---

## Links

- [GitHub Repository](https://github.com/satwareAG/php-firebird)
- [Upstream Issues Analysis](docs/UPSTREAM_ISSUE_ANALYSIS.md)
- [Development History](docs/DEVELOPMENT_HISTORY.md)

[Unreleased]: https://github.com/satwareAG/php-firebird/compare/v7.0.0-rc.6...HEAD
[7.0.0-rc.6]: https://github.com/satwareAG/php-firebird/compare/v7.0.0-rc.5...v7.0.0-rc.6
[7.0.0-rc.5]: https://github.com/satwareAG/php-firebird/compare/v7.0.0-rc.4...v7.0.0-rc.5
[7.0.0-rc.4]: https://github.com/satwareAG/php-firebird/compare/v7.0.0-rc.3...v7.0.0-rc.4
[7.0.0-rc.3]: https://github.com/satwareAG/php-firebird/compare/v7.0.0-rc.2...v7.0.0-rc.3
[7.0.0-rc.2]: https://github.com/satwareAG/php-firebird/compare/v7.0.0-rc.1...v7.0.0-rc.2
[7.0.0-rc.1]: https://github.com/satwareAG/php-firebird/compare/v6.2.0...v7.0.0-rc.1
[6.2.0]: https://github.com/satwareAG/php-firebird/releases/tag/v6.2.0
[1.0.0]: https://github.com/satwareAG/php-firebird/releases/tag/v1.0.0
