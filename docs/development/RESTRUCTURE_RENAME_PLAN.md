# Firebird Extension Restructure & Rename Plan

This document outlines the roadmap for transforming the legacy `interbase` extension into a modern `firebird` extension.

**Status Updated:** December 2025

## Phase 1: Directory Restructure & Renaming ✅ COMPLETE
**Goal:** Rename the extension to `firebird` and reorganize the project structure.

- [x] Rename extension module from `interbase` to `firebird`.
- [x] Rename build arguments (`--with-interbase` -> `--with-firebird`).
- [x] Rename `interbase.c` to `firebird.c`.
- [x] Rename `php_interbase.h` to `php_firebird.h`.
- [x] Update `config.m4` and `config.w32`.
- [x] Ensure `extension=firebird.so` is the target.
- [x] Maintain `fbird_*` as primary functions with `ibase_*` backward compatible aliases.
- [x] Rename all `IBASE_*` constants to `FBIRD_*` (no BC - intentional break).
- [x] Rename all source files: `ibase_*.c` → `fbird_*.c`.

## Phase 2: Namespace & Code Organization ⏳ PARTIAL
**Goal:** Adopt a cleaner folder structure and prepare for OO implementation.

- [ ] Move source files to `src/` directory (optional/standardization).
- [x] Refactor monolithic `interbase.c` into logical units (already done as fbird_*.c files).
- [ ] Establish `Firebird` top-level namespace for future classes.

## Phase 3: Test Suite Modernization ✅ COMPLETE
**Goal:** Ensure tests run against the new extension name.

- [x] Update `.phpt` files to load `firebird` extension.
- [x] Verify all legacy tests pass with the new extension (85 tests, 100% pass).
- [x] Rename test files: `ibase_*.phpt` → `fbird_*.phpt`.

## Phase 4: Object-Oriented API
**Goal:** Provide a modern, object-oriented interface.

- [ ] Implement `Firebird\Connection` class.
- [ ] Implement `Firebird\Statement` class.
- [ ] Implement `Firebird\Result` class.
- [ ] Implement `Firebird\Blob` class.
- [ ] Define interfaces and strict typing.

## Phase 5: Modern Error Handling
**Goal:** Replace legacy warnings with Exceptions in the OO API.

- [ ] Implement `Firebird\Exception`.
- [ ] Ensure OO methods throw exceptions on failure.
- [ ] Keep `ibase_*` functions emitting warnings (or configurable).

## Phase 6: Advanced Features
**Goal:** Support Firebird 4.0/5.0 specific features.

- [ ] Support `INT128` data type.
- [ ] Support `DECFLOAT` data type.
- [ ] Support `TIMESTAMP WITH TIME ZONE`.

## Phase 7: Developer Experience
**Goal:** Improve tooling and distribution.

- [ ] Generate `firebird.stub.php` for IDE autocomplete.
- [ ] Implement Reflection support.
- [ ] Add Composer `composer.json` for installation.

## Risk & Success Criteria
- **Backward Compatibility:** Existing applications using `ibase_*` must continue to work.
- **Stability:** No regressions in core functionality.
- **Adoption:** The new `Firebird\*` API should be the recommended way to use the extension.
