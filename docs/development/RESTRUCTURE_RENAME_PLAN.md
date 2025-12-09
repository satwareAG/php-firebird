# Implementation Plan: Directory Restructure + fbird Rename

**Date:** 2025-12-09
**Author:** Jane Alesi
**Status:** DRAFT

---

## [Overview]

Restructure directory layout and rename from interbase/ibase to fbird extension.

This plan addresses two major changes to the php-firebird extension:
1. **Directory restructure**: Move source files into organized subdirectories to eliminate build artifact pollution in the project root
2. **Extension rename**: Rename from `interbase`/`ibase_*` to `fbird`/`fbird_*` to reflect that this is a Firebird-specific extension (2.5+), not InterBase

**Decision: Restructure FIRST, then Rename**

Rationale:
- Clean directory baseline makes the massive rename easier to review (117 functions, 74 constants)
- Smaller, focused commits for each phase
- Build system changes isolated (config.m4 updates)
- Testing isolation - validate build works after each phase
- Rollback safety - if rename has issues, directory structure is stable

---

## [Types]

No new types or data structures are being introduced.

The rename affects:
- Function prefixes: `ibase_*` → `fbird_*` (117 functions)
- Constant prefixes: `IBASE_*` → `FBIRD_*` (74 constants)
- Internal macros: `PHP_IBASE_*` → `PHP_FBIRD_*`
- Module name: `interbase` → `fbird`

---

## [Files]

### Phase 1: Directory Restructure

**New directories to create:**
- `src/` - All C/C++ source files
- `include/` - All header files

**Files to move:**

| Current Location | New Location |
|------------------|--------------|
| `interbase.c` | `src/fbird_main.c` |
| `ibase_blobs.c` | `src/fbird_blobs.c` |
| `ibase_events.c` | `src/fbird_events.c` |
| `ibase_inspection.c` | `src/fbird_inspection.c` |
| `ibase_metadata.c` | `src/fbird_metadata.c` |
| `ibase_query.c` | `src/fbird_query.c` |
| `ibase_query_exec.c` | `src/fbird_query_exec.c` |
| `ibase_result.c` | `src/fbird_result.c` |
| `ibase_service.c` | `src/fbird_service.c` |
| `php_ibase_udf.c` | `src/fbird_udf.c` |
| `firebird_utils.cpp` | `src/fbird_utils.cpp` |
| `php_interbase.h` | `include/php_fbird.h` |
| `php_ibase_includes.h` | `include/php_fbird_includes.h` |
| `php_ibase_inspection.h` | `include/php_fbird_inspection.h` |
| `php_ibase_query_internal.h` | `include/php_fbird_query_internal.h` |
| `firebird_utils.h` | `include/fbird_utils.h` |
| `firebird_utils_internal.h` | `include/fbird_utils_internal.h` |

**Files to modify:**
- `config.m4` - Update source file paths to use `src/` prefix
- `config.w32` - Update source file paths for Windows build
- `.gitignore` - Ensure build artifacts are properly excluded
- All source files - Update `#include` paths

**Files to keep in root (required by phpize/PECL):**
- `config.m4`
- `config.w32`
- `CREDITS`
- `LICENSE`
- `README.md`
- `CONTRIBUTING.md`

### Phase 2: Extension Rename

**Files affected by rename:**
- All files in `src/` and `include/` (already renamed in Phase 1)
- `config.m4` - Extension name `interbase` → `fbird`
- `config.w32` - Extension name
- `README.md` - Documentation updates
- All test files in `tests/*.phpt` - Function name updates

---

## [Functions]

### Function Rename Mapping (117 functions)

All `ibase_*` functions retain their implementation but get `fbird_*` as primary name.
Legacy `ibase_*` aliases should be provided for backward compatibility.

**Core functions:**
- `ibase_connect` → `fbird_connect` (+ `ibase_connect` alias)
- `ibase_pconnect` → `fbird_pconnect`
- `ibase_close` → `fbird_close`
- `ibase_query` → `fbird_query`
- `ibase_fetch_row` → `fbird_fetch_row`
- `ibase_fetch_assoc` → `fbird_fetch_assoc`
- `ibase_fetch_object` → `fbird_fetch_object`
- ... (117 total)

**Implementation approach:**
1. Internal C function names: `zif_ibase_*` → `zif_fbird_*`
2. PHP function registration: Primary `fbird_*`, alias `ibase_*`
3. Use `PHP_FALIAS()` macro for backward compatibility aliases

---

## [Classes]

No classes to modify. This extension uses procedural functions only.

---

## [Dependencies]

No changes to external dependencies.

**Build system dependencies (unchanged):**
- fbclient library (Firebird client)
- PHP development headers (8.1+)
- autotools (phpize, autoconf)

---

## [Testing]

### Test File Updates

All 85+ PHPT test files in `tests/` need updating:
- Function calls: `ibase_*` → `fbird_*` (or keep as `ibase_*` to test aliases)
- Include paths for any test utilities
- Expected output where function names appear

**Test strategy:**
1. Create a test specifically for backward compatibility aliases
2. Update existing tests to use new `fbird_*` names
3. Run full test suite against PHP 8.1-8.5 matrix
4. Verify all 85 tests pass

### CI/CD Updates

- Update GitHub Actions workflows if paths change
- Verify Docker build scripts work with new structure

---

## [Implementation Order]

**Phase 1: Directory Restructure (Estimated: 4-6 hours)**

1. Create `src/` and `include/` directories
2. Move source files (*.c, *.cpp) to `src/`
3. Move header files (*.h) to `include/`
4. Update `config.m4` source file list
5. Update `config.w32` source file list
6. Update all `#include` statements in source files
7. Update `.gitignore` if needed
8. Test build in Docker container (PHP 8.3 + Firebird 4.0)
9. Run test suite to verify no regressions
10. Commit: `refactor: restructure directory layout (src/, include/)`

**Phase 2: Extension Rename (Estimated: 8-12 hours)**

1. Rename source files: `ibase_*.c` → `fbird_*.c`
2. Rename header files: `php_ibase_*.h` → `php_fbird_*.h`
3. Update `config.m4` extension name and file references
4. Update `config.w32` extension name
5. Rename internal functions: `zif_ibase_*` → `zif_fbird_*`
6. Rename constants: `IBASE_*` → `FBIRD_*`
7. Add backward compatibility aliases using `PHP_FALIAS()`
8. Update all `#include` statements
9. Update `README.md` documentation
10. Update all test files to use `fbird_*` functions
11. Create backward compatibility test
12. Test build in Docker (full matrix)
13. Run full test suite
14. Commit: `feat!: rename extension from interbase to fbird`

**Phase 3: Cleanup and Documentation (Estimated: 2-3 hours)**

1. Update CHANGELOG.md
2. Update any remaining documentation
3. Clean up deprecated/unused code
4. Final test pass
5. Tag release candidate

---

## Appendix: Reference Projects

| Project | Structure | Source Location |
|---------|-----------|-----------------|
| phpredis | Flat | Root directory |
| mongo-php-driver | Organized | `src/` directory |
| **php-firebird (new)** | **Organized** | **`src/`, `include/`** |

---

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| Build breaks after restructure | High | Test incrementally, keep config.m4 in sync |
| Backward compatibility issues | High | Use PHP_FALIAS for ibase_* aliases |
| Test failures from path changes | Medium | Update tests systematically |
| Windows build breaks | Medium | Update config.w32 carefully, test if possible |

---

## Success Criteria

- [ ] All 85+ tests pass
- [ ] Extension builds on PHP 8.1-8.5
- [ ] Extension works with Firebird 2.5, 3.0, 4.0, 5.0
- [ ] `fbird_*` functions work as primary API
- [ ] `ibase_*` functions work as aliases (backward compatibility)
- [ ] No source files in project root
- [ ] Build artifacts contained (not polluting root)
