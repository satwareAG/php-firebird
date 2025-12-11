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

## Key Architectural Decisions

### PHP Version Support
- **Minimum:** PHP 8.1 (enforced via configure-time check)
- **Tested:** PHP 8.1, 8.2, 8.3, 8.4, 8.5

### Firebird Version Support
- **Versions:** Firebird 2.5, 3.0, 4.0, 5.0
- **Conditional compilation:** FB_API_VER >= 30/40 patterns for version-specific features
- **Advanced features:** Timezone support (Firebird 4.0+)

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

## Test Coverage

- **Total tests:** 85 PHPT test files
- **Pass rate:** 100%
- **Coverage areas:** Connection, transactions, queries, blobs, services, metadata

---

*This document consolidates historical development reports from the modernization project.*
*For current development guides, see `docker.md`, `local_qa_workflow.md`, and `BRANCHING.md`.*
