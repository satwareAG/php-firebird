# Implementation Plan: Code Quality & Memory Safety Audit

[Overview]
Comprehensive audit of the PHP-Firebird extension (12,718 LOC) to rename legacy `ib_*` identifiers to `fbird_*`, implement strong UAF detection, and establish ASAN-enabled CI pipeline with automated UAF regression tests.

This audit addresses three critical objectives:
1. **Naming Consistency**: Eliminate all `ib_` prefixes (Interbase heritage) in favor of `fbird_` naming convention
2. **Memory Safety**: Implement defensive programming patterns to detect and prevent Use-After-Free vulnerabilities
3. **Testing Infrastructure**: Establish ASAN/Valgrind CI pipeline and automated UAF regression test suite

The extension manages 8 resource types (link, plink, trans, query, blob, event, service, batch) with complex lifecycle relationships. UAF risks exist at:
- Query parent/child linkage (result resources referencing freed prepared statements)
- Transaction list management (`tr_list` linked list)
- Event callback chains (`event_head` linked list)
- Blob stream data lifecycle

[Types]
No new types are introduced. Existing struct/typedef naming will be audited and renamed.

**Rename Targets (typedef/struct):**
| Current Name | New Name | File |
|--------------|----------|------|
| `ib_query` | `fbird_query` | `include/php_fbird_includes.h:213` |
| `_ib_query` | `_fbird_query` | `include/php_fbird_includes.h:213` |
| `BIND_BUF` | `fbird_bind_buf` | `include/php_fbird_includes.h:185` |

**Variable Naming Pattern Changes:**
- `ib_link` → `fbird_link`
- `ib_trans` → `fbird_trans`
- `ib_query` → `fbird_qry` (to avoid type name collision)
- `ib_blob` → `fbird_blob_var`
- `ib_event` → `fbird_evt`
- `ib_service` → `fbird_svc`
- `ib_batch` → `fbird_bat`

[Files]
Files to be modified for naming consistency and UAF hardening.

**Phase 1: Header Files (Naming + UAF Macros)**
- `include/php_fbird_includes.h` - Rename `_ib_query`, add UAF guard macros
- `include/php_firebird.h` - Update extern declarations
- `include/php_fbird_query_internal.h` - Rename function signatures
- `include/php_fbird_query_prepare.h` - Rename function signatures
- `include/php_fbird_query_bind.h` - Rename function signatures
- `include/php_fbird_query_array.h` - Rename function signatures
- `include/php_fbird_inspection.h` - Rename function signatures

**Phase 2: Source Files (Naming Refactor)**
- `src/firebird.c` (3,780 lines) - Core connection/transaction management
- `src/fbird_query_exec.c` (1,693 lines) - Query execution
- `src/fbird_blobs.c` (1,246 lines) - BLOB handling
- `src/fbird_query_bind.c` (1,165 lines) - Parameter binding
- `src/fbird_result.c` (929 lines) - Result set handling
- `src/fbird_service.c` (690 lines) - Service API
- `src/fbird_metadata.c` (641 lines) - Metadata retrieval
- `src/fbird_events.c` (608 lines) - Database events
- `src/fbird_query_array.c` (544 lines) - Array handling
- `src/fbird_query_prepare.c` (519 lines) - Statement preparation
- `src/fbird_inspection.c` (502 lines) - Database inspection
- `src/fbird_datetime.c` (401 lines) - Date/time handling

**Phase 3: UAF Hardening**
- `include/php_fbird_includes.h` - Add `FBIRD_RESOURCE_MAGIC` validation macros
- All source files - Add magic number validation at resource access points

**Phase 4: Test Infrastructure**
- `scripts/run-asan-ci.sh` (NEW) - ASAN-enabled CI runner
- `scripts/uaf-regression-suite.sh` (NEW) - Automated UAF test runner
- `.gitlab-ci.yml` - Add ASAN/Valgrind stages
- `tests/uaf_*.phpt` (NEW) - UAF regression test files

[Functions]
Function naming changes and new UAF helper functions.

**Rename Pattern: `_php_fbird_*` (internal) and `fbird_*` (public)**
All functions already follow this convention. No function renames needed.

**Variable Parameter Renames Within Functions:**
Each function that uses `ib_*` variable names will have them renamed to `fbird_*`.

**New UAF Helper Functions (in `include/php_fbird_includes.h`):**
```c
// Magic number validation for resource integrity
#define FBIRD_MAGIC_LINK    0xFB01LINK
#define FBIRD_MAGIC_TRANS   0xFB02TRAN
#define FBIRD_MAGIC_QUERY   0xFB03QURY
#define FBIRD_MAGIC_BLOB    0xFB04BLOB
#define FBIRD_MAGIC_EVENT   0xFB05EVNT
#define FBIRD_MAGIC_SERVICE 0xFB06SRVC
#define FBIRD_MAGIC_BATCH   0xFB07BATC
#define FBIRD_MAGIC_FREED   0xDEADFB1D

// Validation macro
#define FBIRD_VALIDATE_MAGIC(ptr, expected_magic) \
    do { \
        if ((ptr)->magic != (expected_magic)) { \
            if ((ptr)->magic == FBIRD_MAGIC_FREED) { \
                zend_throw_exception(zend_ce_error, \
                    "Use-after-free: resource was already freed", 0); \
                RETURN_THROWS(); \
            } \
            zend_throw_exception(zend_ce_error, \
                "Invalid resource magic number", 0); \
            RETURN_THROWS(); \
        } \
    } while(0)
```

**Modified Struct Signatures (add magic field):**
```c
typedef struct {
    uint32_t magic;  // NEW: FBIRD_MAGIC_LINK
    fb_safe_handle handle;
    // ... existing fields
} fbird_db_link;
```

[Classes]
No classes in this C extension. N/A.

[Dependencies]
No new dependencies required.

**Build Dependencies (existing):**
- Firebird 3.0+ client library
- PHP 8.3+ development headers
- Valgrind (for testing)
- GCC/Clang with ASan support

**Docker Container Requirements:**
- `php83-dev` container updated with ASan-enabled PHP build
- Valgrind installed in test containers

[Testing]
Comprehensive testing approach with UAF regression suite.

**Phase 1: Verify Existing Tests Pass After Rename**
- Run full test suite: `make test`
- Verify no regressions from variable renaming

**Phase 2: UAF Regression Test Suite**
Create new test files in `tests/`:

| Test File | Purpose |
|-----------|---------|
| `tests/uaf_query_after_free.phpt` | Verify TypeError on fetch after fbird_free_query |
| `tests/uaf_trans_after_commit.phpt` | Verify TypeError on trans use after commit |
| `tests/uaf_blob_after_close.phpt` | Verify safe handling of blob after close |
| `tests/uaf_event_after_free.phpt` | Verify event cleanup doesn't double-free |
| `tests/uaf_result_parent_freed.phpt` | Verify result handles parent query being freed |
| `tests/uaf_pconnect_cache.phpt` | Verify persistent connection cache integrity |
| `tests/uaf_fork_safety.phpt` | Verify forked child doesn't corrupt parent resources |

**Phase 3: ASAN CI Pipeline**
```yaml
# .gitlab-ci.yml addition
test-asan:
  stage: test
  image: php:8.3-cli
  variables:
    USE_ZEND_ALLOC: 0
    ZEND_DONT_UNLOAD_MODULES: 1
  before_script:
    - phpize && ./configure CFLAGS="-fsanitize=address -g -O1" LDFLAGS="-fsanitize=address"
    - make clean && make
  script:
    - ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 make test TESTS="tests/"
  allow_failure: false
```

**Phase 4: Valgrind CI Pipeline**
```yaml
test-valgrind:
  stage: test
  image: php:8.3-cli
  before_script:
    - apt-get update && apt-get install -y valgrind
    - phpize && ./configure && make clean && make
  script:
    - ./scripts/run-valgrind.sh --quick
  allow_failure: false
```

[Implementation Order]
Ordered implementation sequence to minimize conflicts and ensure incremental validation.

**Week 1: Foundation**
1. Create UAF guard macros in `include/php_fbird_includes.h`
2. Add magic field to all resource structs
3. Update valgrind suppression file for new magic patterns
4. Implement ASAN CI pipeline script

**Week 2: Header Renames**
5. Rename `_ib_query` to `_fbird_query` in `php_fbird_includes.h`
6. Rename `BIND_BUF` to `fbird_bind_buf`
7. Update all header function signatures
8. Verify build succeeds

**Week 3: Core Source Renames (Large Files)**
9. Refactor `src/firebird.c` - rename all `ib_*` variables
10. Refactor `src/fbird_query_exec.c` - rename all `ib_*` variables
11. Run full test suite - verify no regressions

**Week 4: Query Module Renames**
12. Refactor `src/fbird_query_bind.c`
13. Refactor `src/fbird_query_prepare.c`
14. Refactor `src/fbird_query_array.c`
15. Run full test suite

**Week 5: Supporting Modules Renames**
16. Refactor `src/fbird_blobs.c`
17. Refactor `src/fbird_result.c`
18. Refactor `src/fbird_events.c`
19. Refactor `src/fbird_service.c`
20. Run full test suite

**Week 6: Remaining Modules + UAF Tests**
21. Refactor `src/fbird_metadata.c`
22. Refactor `src/fbird_inspection.c`
23. Refactor `src/fbird_datetime.c`
24. Create UAF regression test suite (7 new tests)
25. Run ASAN CI pipeline locally

**Week 7: CI Integration + Final Validation**
26. Integrate ASAN stage into `.gitlab-ci.yml`
27. Integrate Valgrind stage into `.gitlab-ci.yml`
28. Full test matrix run (PHP 8.3/8.4, Firebird 3.0/4.0/5.0)
29. Update CHANGELOG.md
30. Create release candidate

**Commit Strategy:**
- Each source file rename is a separate commit (<200 LOC changes)
- UAF macro additions are a single commit
- Each new test file is a separate commit
- CI pipeline changes are a single commit

**Validation Points:**
- After each source file rename: `make && make test`
- After UAF macros: `make && make test TESTS="tests/use_after_free*"`
- After ASAN integration: Full CI pipeline run

---
*Plan created: 2026-01-05*
*Estimated effort: 7 weeks (part-time)*
*Risk level: Medium (naming changes affect all modules)*
