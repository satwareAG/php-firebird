# Implementation Plan: Source/Build Directory Separation

[Overview]
Restructure php-firebird to separate source files from build artifacts following C/C++ and PHP extension best practices.

This refactoring addresses the current state where source files (.c, .h, .cpp) are mixed with build-generated artifacts (autoconf files, object files, Makefiles) in the project root. The goal is to achieve:
- Clean source directory that stays pristine after builds
- Out-of-source build capability for multiple configurations (Debug/Release/ASAN)
- Easier `git status` (no noise from generated files)
- Professional project structure matching industry standards

**Current Layout (Mixed):**
```
php-firebird/
├── firebird.c          # Source (root)
├── fbird_*.c           # Source (root)
├── php_*.h             # Headers (root)
├── config.m4           # Build system
├── autom4te.cache/     # Generated
├── modules/            # Generated .so
├── Makefile            # Generated
└── tests/              # Tests (stays)
```

**Target Layout (Separated):**
```
php-firebird/
├── src/                # All .c and .cpp sources
│   ├── firebird.c
│   ├── fbird_blobs.c
│   └── ...
├── include/            # All .h headers  
│   ├── php_firebird.h
│   └── ...
├── tests/              # PHPT tests (unchanged location)
├── config.m4           # Updated paths
├── build/              # All build artifacts (gitignored)
└── docs/, scripts/     # Unchanged
```

[Types]
No type definitions are changed in this refactoring.

This is a structural reorganization only. All C structs, typedefs, enums, and PHP resource types remain unchanged. The refactoring affects only file locations and include paths.

[Files]
Move 13 source files, 11 header files, update config.m4, and modify scripts.

**New Directories:**
- `src/` - All C/C++ implementation files
- `include/` - All header files (public and internal)

**Files to MOVE to `src/`:**
1. `firebird.c` → `src/firebird.c`
2. `fbird_blobs.c` → `src/fbird_blobs.c`
3. `fbird_datetime.c` → `src/fbird_datetime.c`
4. `fbird_events.c` → `src/fbird_events.c`
5. `fbird_inspection.c` → `src/fbird_inspection.c`
6. `fbird_metadata.c` → `src/fbird_metadata.c`
7. `fbird_query_array.c` → `src/fbird_query_array.c`
8. `fbird_query_bind.c` → `src/fbird_query_bind.c`
9. `fbird_query_exec.c` → `src/fbird_query_exec.c`
10. `fbird_query_prepare.c` → `src/fbird_query_prepare.c`
11. `fbird_result.c` → `src/fbird_result.c`
12. `fbird_service.c` → `src/fbird_service.c`
13. `firebird_utils.cpp` → `src/firebird_utils.cpp`

**Files to MOVE to `include/`:**
1. `php_firebird.h` → `include/php_firebird.h`
2. `php_fbird_includes.h` → `include/php_fbird_includes.h`
3. `php_fbird_inspection.h` → `include/php_fbird_inspection.h`
4. `php_fbird_query_array.h` → `include/php_fbird_query_array.h`
5. `php_fbird_query_bind.h` → `include/php_fbird_query_bind.h`
6. `php_fbird_query_internal.h` → `include/php_fbird_query_internal.h`
7. `php_fbird_query_prepare.h` → `include/php_fbird_query_prepare.h`
8. `fbird_datetime.h` → `include/fbird_datetime.h`
9. `firebird_utils.h` → `include/firebird_utils.h`
10. `firebird_utils_internal.h` → `include/firebird_utils_internal.h`
11. `src/php_fbird_compat.h` → `include/php_fbird_compat.h` (consolidate)

**Files to CONSOLIDATE:**
- `src/cpp/fb_blr_compat.h` → `include/cpp/fb_blr_compat.h`
- `src/Firebird/` directory → Keep as-is (Firebird SDK headers)

**Files to MODIFY:**
1. `config.m4` - Update source file paths
2. `config.w32` - Update source file paths for Windows
3. `.gitignore` - Add build artifact patterns
4. `scripts/build.sh` - Update if any direct file references
5. `scripts/qa.sh` - Update PHPCS src/ path reference
6. All moved `.c` files - Update `#include` paths

**Build Artifacts Location (.gitignore):**
- `build/` - All autoconf/make artifacts
- `modules/` - Compiled extensions (already gitignored)
- Test artifacts remain in `tests/` (already gitignored: *.diff, *.exp, *.log, *.out)

[Functions]
No function signatures change.

All existing function implementations remain identical. Only file locations and include directives are modified.

**Include Path Updates Required:**
- `#include "php_firebird.h"` → `#include "include/php_firebird.h"` OR use `-I include/` in config.m4
- `#include "src/php_fbird_compat.h"` → `#include "include/php_fbird_compat.h"`

The preferred approach is to add `-I include/` to compiler flags in config.m4 so include statements can remain as `#include "php_firebird.h"` without path prefixes.

[Classes]
No class changes.

The PHP extension contains no C++ classes. The `firebird_utils.cpp` file uses C++ standard library only (std::string, std::vector) without custom classes.

[Dependencies]
No dependency changes required.

The build system (autoconf/phpize) and runtime dependencies remain unchanged:
- PHP 8.1+ development headers
- Firebird client library (libfbclient)
- autoconf, automake, libtool

[Testing]
Test structure unchanged; artifact location configurable via PHP run-tests.php.

**Test File Location:**
- `tests/` directory stays at project root (convention for PHP extensions)
- All 137 PHPT test files remain in place

**Test Artifacts:**
Currently generated in `tests/` alongside source:
- `tests/*.diff` - Difference files (failure)
- `tests/*.exp` - Expected output
- `tests/*.log` - Test logs
- `tests/*.out` - Actual output

**Relocation Option (Optional Enhancement):**
PHP's run-tests.php supports `--temp-source` and `--temp-target` for relocating artifacts:
```bash
php run-tests.php --temp-target=/tmp/php-test-artifacts tests/
```

This can be added to `scripts/test.sh` if desired, but is not required since `.gitignore` already excludes these artifacts.

**Verification:**
- All 137 PHPT tests must pass after restructuring
- CI pipeline must succeed
- Docker builds must work (scripts use `/ext` mount point which is unaffected)

[Implementation Order]
Sequential restructuring with verification at each step to minimize risk.

1. **Create directory structure**
   - `mkdir -p src include include/cpp`
   - Verify directories exist

2. **Move C source files to src/**
   - `git mv firebird.c src/`
   - `git mv fbird_*.c src/`
   - `git mv firebird_utils.cpp src/`
   - Do NOT compile yet

3. **Move header files to include/**
   - `git mv php_*.h include/`
   - `git mv fbird_datetime.h include/`
   - `git mv firebird_utils*.h include/`
   - `git mv src/php_fbird_compat.h include/`
   - `git mv src/cpp/fb_blr_compat.h include/cpp/`
   - Remove empty `src/cpp/` if applicable

4. **Update config.m4**
   - Change `PHP_NEW_EXTENSION` source list to `src/` paths
   - Add `PHP_ADD_INCLUDE([$ext_dir/include])` for header search path
   - Update `PHP_ADD_SOURCES` for C++ files

5. **Update config.w32 (Windows)**
   - Mirror changes from config.m4

6. **Update include statements in source files**
   - Option A: Add `-I include/` to config.m4 (minimal source changes)
   - Option B: Update all includes to use `include/` prefix
   - Recommend Option A for cleaner source

7. **Update .gitignore**
   - Ensure build artifacts are fully covered
   - Add any new patterns needed

8. **Update scripts**
   - `scripts/qa.sh`: Verify PHPCS path still works (`src/` → `src/Firebird/` for stubs)
   - Other scripts use `/ext` in Docker, unaffected by internal restructure

9. **Test build in Docker**
   ```bash
   docker compose exec -T php83-dev bash -c "cd /ext && phpize && ./configure && make -j4"
   ```

10. **Run full test suite**
    ```bash
    docker compose exec -T php83-dev bash -c "cd /ext && make test"
    ```

11. **Verify CI pipeline**
    - Push to feature branch
    - Monitor GitLab CI

12. **Update documentation**
    - README.md (if build instructions change)
    - CONTRIBUTING.md (developer setup)

13. **Clean up and commit**
    - Verify no stray files
    - Create descriptive commit message

**Rollback Strategy:**
If issues arise, `git reset --hard` to pre-restructure commit. The restructuring uses `git mv` to preserve history.

**Scripts Requiring NO Changes:**
- `scripts/build.sh` - Uses relative paths, unaffected
- `scripts/test.sh` - Runs from project root
- `scripts/coverage.sh` - Uses make targets
- `scripts/debug_segfault.sh` - Docker paths
- Most scripts reference `/ext` (Docker mount) not source files directly

**Scripts Requiring Review:**
- `scripts/qa.sh` line: `vendor/bin/phpcs $PHPCS_OPTS src/` - This refers to `src/Firebird/` PHP stubs, not C sources. May need path update if PHP stubs move.
