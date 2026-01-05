# Implementation Plan: Service API Test Coverage

## [Overview]

Add comprehensive PHPT tests to increase Service API coverage from 0% (fb_service.hpp) and improve fbird_service.c coverage to achieve ≥80% overall coverage for service manager functionality.

The Firebird Service API provides administrative capabilities including backup/restore operations, user management, database maintenance, and server information retrieval. Currently, the C++ OO API wrapper in `src/cpp/fb_service.hpp` has 0% test coverage (0/174 lines), while the C implementation in `fbird_service.c` has 72% coverage (251/349 lines). Existing tests (`fbird_service_user.phpt`, `fbird_service_db_mgr.phpt`) cover basic functionality but don't exercise error paths, edge cases, or comprehensive option combinations.

This implementation will create 3 new comprehensive test files focusing on backup/restore operations, additional user management scenarios, and database maintenance operations. The tests will exercise both the legacy C API and the OO API wrapper paths to ensure complete coverage.

## [Types]

Test data structures and constants validation for service operations.

The tests will validate all service-related constants defined in `php_fbird_service_minit()`:

**Backup Operation Constants:**
- `FBIRD_BKP_IGNORE_CHECKSUMS` (isc_spb_bkp_ignore_checksums)
- `FBIRD_BKP_IGNORE_LIMBO` (isc_spb_bkp_ignore_limbo)
- `FBIRD_BKP_METADATA_ONLY` (isc_spb_bkp_metadata_only)
- `FBIRD_BKP_NO_GARBAGE_COLLECT` (isc_spb_bkp_no_garbage_collect)
- `FBIRD_BKP_OLD_DESCRIPTIONS` (isc_spb_bkp_old_descriptions)
- `FBIRD_BKP_NON_TRANSPORTABLE` (isc_spb_bkp_non_transportable)
- `FBIRD_BKP_CONVERT` (isc_spb_bkp_convert)

**Restore Operation Constants:**
- `FBIRD_RES_DEACTIVATE_IDX` (isc_spb_res_deactivate_idx)
- `FBIRD_RES_NO_SHADOW` (isc_spb_res_no_shadow)
- `FBIRD_RES_NO_VALIDITY` (isc_spb_res_no_validity)
- `FBIRD_RES_ONE_AT_A_TIME` (isc_spb_res_one_at_a_time)
- `FBIRD_RES_REPLACE` (isc_spb_res_replace)
- `FBIRD_RES_CREATE` (isc_spb_res_create)
- `FBIRD_RES_USE_ALL_SPACE` (isc_spb_res_use_all_space)

**Maintenance Constants:** FBIRD_PRP_*, FBIRD_RPR_*, FBIRD_STS_*, FBIRD_SVC_*

## [Files]

New test files to be created in tests/coverage/ directory.

**New Files:**

1. `tests/coverage/service_backup_restore.phpt`
   - Purpose: Test backup and restore operations with various option combinations
   - Coverage target: `_php_fbird_backup_restore()`, `fbird_backup()`, `fbird_restore()`
   - Tests: Metadata-only backup, full backup, restore with options, error handling

2. `tests/coverage/service_user_advanced.phpt`
   - Purpose: Test user management edge cases and error paths
   - Coverage target: `_php_fbird_user()`, user validation, credential edge cases
   - Tests: Long usernames (63 char limit), long passwords (255 char limit), special characters

3. `tests/coverage/service_maintenance_operations.phpt`
   - Purpose: Test database maintenance operations and server info retrieval
   - Coverage target: `_php_fbird_service_action()`, all maintenance constants
   - Tests: Sweep interval, shutdown/online, write modes, access modes, repair operations

**Files to Modify:**

None. Existing test infrastructure in `tests/firebird.inc` and `tests/functions.inc` is sufficient.

## [Functions]

Service API functions requiring comprehensive test coverage.

**Core Service Functions (fbird_service.c):**

1. `PHP_FUNCTION(fbird_service_attach)` - Lines 246-338
   - Current coverage: Basic connection tested
   - Gaps: INI defaults fallback (lines 261-277), buffer overflow checks (lines 279-291)
   - Test scenarios: Default credentials, hostname edge cases, NULL parameters

2. `PHP_FUNCTION(fbird_service_detach)` - Lines 340-353
   - Current coverage: Basic disconnect tested
   - Gaps: Invalid resource handling
   - Test scenarios: Detach invalid service, double detach

3. `PHP_FUNCTION(fbird_backup)` - Lines 557-560
   - Current coverage: Basic backup tested in existing tests
   - Gaps: All backup options, verbose mode, error paths
   - Test scenarios: Each FBIRD_BKP_* constant, combined options, invalid paths

4. `PHP_FUNCTION(fbird_restore)` - Lines 562-565
   - Current coverage: Basic restore tested
   - Gaps: All restore options, replace mode, error paths
   - Test scenarios: Each FBIRD_RES_* constant, combined options, invalid paths

5. `PHP_FUNCTION(fbird_maintain_db)` - Lines 658-661
   - Current coverage: Basic maintenance tested
   - Gaps: All maintenance actions, repair operations
   - Test scenarios: All FBIRD_PRP_* and FBIRD_RPR_* constants

6. `PHP_FUNCTION(fbird_db_info)` - Lines 663-666
   - Current coverage: Header pages tested
   - Gaps: Other info types (data pages, index pages, system relations)
   - Test scenarios: All FBIRD_STS_* constants

7. `PHP_FUNCTION(fbird_server_info)` - Lines 668-684
   - Current coverage: Get users tested
   - Gaps: Version, implementation, environment variables
   - Test scenarios: All FBIRD_SVC_* constants

**Helper Functions:**

8. `_php_fbird_user()` - Lines 184-229
   - Current coverage: Basic add/modify/delete tested
   - Gaps: Buffer overflow checks, parameter validation
   - Test scenarios: Maximum length username/password, NULL/empty parameters

9. `_php_fbird_backup_restore()` - Lines 504-555
   - Current coverage: Basic operation tested
   - Gaps: Verbose mode, all option flags, SPB buffer validation
   - Test scenarios: Verbose output parsing, option combinations

10. `_php_fbird_service_action()` - Lines 567-656
    - Current coverage: Property and stats actions tested
    - Gaps: Repair actions, write mode, access mode, reserve space
    - Test scenarios: All switch cases in lines 573-591

11. `_php_fbird_service_query()` - Lines 355-502
    - Current coverage: Basic query tested
    - Gaps: Heap buffer expansion (lines 348-355), all info types
    - Test scenarios: Large result output, all switch cases

**OO API Wrapper Functions (src/cpp/fb_service.hpp):**

12. `fbsvc_attach()` - Lines 311-343
    - Current coverage: 0% (not called by C code yet)
    - Test approach: Indirectly via future OO API migration

13. `fbsvc_detach()` - Lines 348-358
    - Current coverage: 0%

14. `fbsvc_start()` - Lines 363-382
    - Current coverage: 0%

15. `fbsvc_query()` - Lines 387-411
    - Current coverage: 0%

16. `fb::ServiceWrapper::attach()` - Lines 79-140
    - Current coverage: 0%

17. `fb::ServiceWrapper::detach()` - Lines 148-185
    - Current coverage: 0%

18. `fb::ServiceWrapper::start()` - Lines 195-229
    - Current coverage: 0%

19. `fb::ServiceWrapper::query()` - Lines 243-280
    - Current coverage: 0%

**Note:** OO API wrapper functions have 0% coverage because `fbird_service.c` still uses legacy `isc_service_*` functions. Full OO API migration is Phase 8 (future work). Current tests will improve C API coverage to ≥80%.

## [Classes]

C++ Service API wrapper classes requiring test coverage.

**fb::ServiceWrapper** (src/cpp/fb_service.hpp, lines 35-295)

Class members:
- `Firebird::IService* m_service` - Service handle
- `Firebird::IMaster* m_master` - Master interface

Public methods:
- `ServiceWrapper()` - Constructor (line 37)
- `~ServiceWrapper()` - Destructor (lines 39-44)
- `attach()` - Connect to service manager (lines 79-140)
- `detach()` - Disconnect (lines 148-185)
- `start()` - Start service task (lines 195-229)
- `query()` - Query service status (lines 243-280)
- `isAttached()` - Check connection status (line 285)
- `get()` - Get raw IService pointer (line 290)

**Current status:** Class exists but has 0% coverage because the C code in `fbird_service.c` uses legacy `isc_service_attach()` instead of calling `fbsvc_attach()` wrapper.

**Test strategy:** 
- Current implementation: Test legacy C API paths in `fbird_service.c`
- Future implementation (Phase 8): Migrate `fbird_service.c` to use OO API wrappers, then verify coverage increases

## [Dependencies]

No new dependencies required.

Existing test infrastructure dependencies:
- `tests/firebird.inc` - Database connection and initialization
- `tests/skipif.inc` - Extension availability checks
- `tests/functions.inc` - Test utility functions
- Docker environment - Firebird service containers (firebird30, firebird40, firebird50)

Service API requires:
- Service manager credentials (SYSDBA/masterkey)
- Temporary database files for backup/restore testing
- Write permissions to filesystem for backup file creation

## [Testing]

Create 3 new PHPT tests to achieve ≥80% coverage of Service API.

**Test File 1: tests/coverage/service_backup_restore.phpt**
- Test all backup options (7 constants): IGNORE_CHECKSUMS, IGNORE_LIMBO, METADATA_ONLY, NO_GARBAGE_COLLECT, OLD_DESCRIPTIONS, NON_TRANSPORTABLE, CONVERT
- Test all restore options (7 constants): DEACTIVATE_IDX, NO_SHADOW, NO_VALIDITY, ONE_AT_A_TIME, REPLACE, CREATE, USE_ALL_SPACE
- Test option combinations (bitwise OR)
- Test verbose mode output parsing
- Test error paths: invalid paths, invalid service handle, permission errors
- Coverage target: `_php_fbird_backup_restore()` lines 504-555

**Test File 2: tests/coverage/service_user_advanced.phpt**
- Test maximum username length (63 chars)
- Test maximum password length (255 chars)  
- Test buffer overflow protection (>63 char username, >255 char password)
- Test special characters in names
- Test NULL/empty parameter handling
- Test modify with partial parameters (only password, only names)
- Coverage target: `_php_fbird_user()` lines 184-229, buffer validation lines 279-291

**Test File 3: tests/coverage/service_maintenance_operations.phpt**
- Test all property constants: PAGE_BUFFERS, SWEEP_INTERVAL, SHUTDOWN_DB, DENY_NEW_TRANSACTIONS, DENY_NEW_ATTACHMENTS, SET_SQL_DIALECT, ACTIVATE, DB_ONLINE
- Test all repair constants: CHECK_DB, IGNORE_CHECKSUM, KILL_SHADOWS, MEND_DB, VALIDATE_DB, SWEEP_DB
- Test write mode options: WM_ASYNC, WM_SYNC
- Test access mode options: AM_READONLY, AM_READWRITE  
- Test reserve space options: RES_USE_FULL, RES
- Test all stats constants: DATA_PAGES, DB_LOG, HDR_PAGES, IDX_PAGES, SYS_RELATIONS
- Test all server info constants: SERVER_VERSION, IMPLEMENTATION, GET_ENV, SVR_DB_INFO
- Coverage target: `_php_fbird_service_action()` lines 567-656, all switch cases

**Validation Strategy:**
1. Run tests locally with coverage enabled: `CFLAGS="--coverage" ./configure && make && make test`
2. Generate coverage report: `lcov --capture --directory . --output-file coverage.info`
3. Verify fb_service.hpp coverage increases (target: ≥80%)
4. Verify fbird_service.c coverage increases (target: ≥80%)
5. Run with sanitizers to detect memory issues
6. Run with Valgrind to detect leaks

**Test Execution Requirements:**
- Firebird service must be running
- Temporary directories for backup files
- Service manager credentials
- Skip tests gracefully if service unavailable

## [Implementation Order]

Implement tests in logical dependency order to validate incrementally.

**Step 1:** Create test infrastructure validation script
- Verify service manager is accessible
- Check temp directory write permissions
- Validate test database exists

**Step 2:** Implement `tests/coverage/service_user_advanced.phpt`
- Rationale: User management is self-contained, doesn't require backup files
- Tests: Username/password boundary conditions, validation logic
- Expected coverage gain: ~30 lines in `_php_fbird_user()`

**Step 3:** Implement `tests/coverage/service_backup_restore.phpt`
- Rationale: Backup/restore is core service functionality
- Tests: All backup/restore options, verbose mode, error paths
- Expected coverage gain: ~50 lines in `_php_fbird_backup_restore()`, helper functions

**Step 4:** Implement `tests/coverage/service_maintenance_operations.phpt`
- Rationale: Maintenance operations cover the most diverse code paths
- Tests: All maintenance, repair, and info constants
- Expected coverage gain: ~90 lines in `_php_fbird_service_action()`, query logic

**Step 5:** Run coverage analysis
- Generate lcov report
- Identify remaining gaps
- Create targeted mini-tests if needed

**Step 6:** Add edge case tests if coverage < 80%
- Test concurrent service connections
- Test service operations during transaction
- Test cleanup during shutdown (already covered by bug_issue56_service_shutdown.phpt)

**Step 7:** Validate with memory analysis
- Run tests under AddressSanitizer
- Run tests under Valgrind
- Ensure no memory leaks or errors

**Step 8:** CI validation
- Run full test suite locally: `./scripts/test_with_act.sh --full`
- Verify coverage threshold met: `./scripts/test_with_act.sh --coverage`
- Submit PR when all checks pass
