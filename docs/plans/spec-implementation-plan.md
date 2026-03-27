---
description: >-
  Consolidated implementation plan for v10.0.0 with phases,
  dependencies, ordering, and effort estimates.
tags: [v10, implementation-plan, phases, dependencies]
priority: 1
---

# v10.0.0 Implementation Plan

## Dependency Graph

```text
Phase 0 (BLOCKER)          Phase 1 (Foundation)       Phase 2 (Modernization)
spec-132 ──────────────>  spec-120 ──────────────>  spec-void-star
  legacy wrappers            typed objects              type safety
                                │                         │
                                v                         v
                          spec-legacy-removal ─────>  spec-features
                            cleanup deps               DECFLOAT/INT128
                                                         PDO batch
```

## Phase 0: Build Fix (#132) - BLOCKER

**Spec**: `specs/spec-132-legacy-wrappers.md`
**Branch**: `fix/v10-p0-legacy-handle-events`
**Effort**: 2-3 days
**Blocks**: Everything

| Step | Task | Files | Est |
|------|------|-------|-----|
| 0.1 | Read `fbird_metadata.c` to verify struct layout | - | 30m |
| 0.2 | Create `firebird_legacy_wrappers.h` | New | 2h |
| 0.3 | Create `firebird_legacy_wrappers.c` (fbb_\*) | New | 3h |
| 0.4 | Add fbsvc_\*, fbe_\*, fbc_/fbt_ wrappers | New | 2h |
| 0.5 | Add fbm_\* wrappers (metadata struct access) | New | 2h |
| 0.6 | Add fbs_\*, fbxpb_\*, fbu_\*_tz wrappers | New | 2h |
| 0.7 | Update `config.m4` | config.m4 | 15m |
| 0.8 | Fix one-line issues (remove include, dup define, typo) | 3 files | 15m |
| 0.9 | Add `#include "firebird_legacy_wrappers.h"` to 15 .c files | 15 files | 1h |
| 0.10 | Build iteratively, fix compile errors | All | 4h |
| 0.11 | Run full test suite | - | 1h |
| 0.12 | Run `bash scripts/check-stubs-sync.sh` | - | 5m |

**Exit Gate**: `build.sh` exits 0, 247 tests pass, stubs sync clean.

## Phase 1: Foundation (Typed Objects + Legacy Cleanup)

**Specs**: `specs/spec-120-typed-connection.md`, `specs/spec-v10-legacy-removal.md`
**Branch**: `feat/v10-typed-connection`
**Effort**: 5-7 days
**Depends on**: Phase 0 merged to satware-main

### 1A: Legacy Cleanup (do first, reduces noise for 1B)

| Step | Task | Est |
|------|------|-----|
| 1A.1 | Remove `legacy_handle_*` functions from fbird_classes.c | 2h |
| 1A.2 | Remove declarations from fbird_classes_internal.h | 30m |
| 1A.3 | Remove v8 compat macros from firebird.c | 30m |
| 1A.4 | Inline isc_* calls in fbird_events.c | 3h |
| 1A.5 | Remove multi-db transaction handling | 2h |
| 1A.6 | Remove deprecated param_info path | 1h |
| 1A.7 | Build + test | 1h |

### 1B: Typed Connection Objects

| Step | Task | Est |
|------|------|-----|
| 1B.1 | Update `Firebird\Connection` class to wrap `fbird_db_link*` | 3h |
| 1B.2 | Update `fbird_connect()` return path to return object | 2h |
| 1B.3 | Create dual-accept ZPP helper (`_php_fbird_get_link()`) | 2h |
| 1B.4 | Update all 87 `fbird_*` functions to use dual-accept | 4h |
| 1B.5 | Port Transaction, Statement, ResultSet, Blob to typed objects | 6h |
| 1B.6 | Update PDO driver to extract from objects | 2h |
| 1B.7 | Update stubs (firebird-stubs.php + phpstan/fbird.stub.php) | 2h |
| 1B.8 | Add new tests (object return, compat, interop) | 3h |
| 1B.9 | Run `check-stubs-sync.sh`, full test suite | 1h |

**Exit Gate**: `fbird_connect()` returns `Firebird\Connection`, no `legacy_handle_*` remain, 247+ tests pass.

## Phase 2: Modernization (Type Safety + Features)

**Specs**: `specs/spec-v10-void-star-elimination.md`, `specs/spec-v10-features.md`
**Branch**: `feat/v10-type-safety` then `feat/v10-features`
**Effort**: 4-6 days
**Depends on**: Phase 1 merged to satware-main

### 2A: void* Elimination

| Step | Task | Est |
|------|------|-----|
| 2A.1 | Create `firebird_utils_typed.h` with opaque structs | 2h |
| 2A.2 | Migrate fbird_connection.c | 1h |
| 2A.3 | Migrate fbird_transaction.c | 1h |
| 2A.4 | Migrate fbird_query_*.c (3 files) | 2h |
| 2A.5 | Migrate fbird_result.c, fbird_blobs.c, fbird_batch.c | 2h |
| 2A.6 | Migrate fbird_service.c, fbird_events.c | 1h |
| 2A.7 | Migrate pdo_fbird/*.c (2 files) | 1h |
| 2A.8 | Build + test | 1h |

### 2B: DECFLOAT/INT128 Types

| Step | Task | Est |
|------|------|-----|
| 2B.1 | Add DECFLOAT detection in fbird_result.c | 2h |
| 2B.2 | Add INT128 string conversion in fbird_result.c | 1h |
| 2B.3 | Update fbird_field_info() subtype mapping | 1h |
| 2B.4 | Write decfloat_001.phpt, int128_002.phpt | 2h |
| 2B.5 | Update stubs | 30m |

### 2C: PDO Batch DML

| Step | Task | Est |
|------|------|-----|
| 2C.1 | Implement statement splitter (quote-aware) | 2h |
| 2C.2 | Update `fbird_pdo_exec()` | 1h |
| 2C.3 | Write pdo_batch_dml.phpt, pdo_batch_dml_error.phpt | 2h |
| 2C.4 | Update stubs | 30m |

**Exit Gate**: All v10 features implemented, stubs synced, 250+ tests pass.

## Phase 3: Release

**Effort**: 1 day
**Depends on**: Phase 2 merged

| Step | Task |
|------|------|
| 3.1 | Update VERSION to 10.0.0 |
| 3.2 | Write CHANGELOG.md [10.0.0] section |
| 3.3 | Update README.md badge to blue (stable) |
| 3.4 | Update stubs/composer.json branch-alias |
| 3.5 | Run `check-stubs-sync.sh` |
| 3.6 | Full test matrix (PHP 8.3, 8.4) |
| 3.7 | Tag and push |

## Summary

| Phase | Specs | Effort | Blocks |
|-------|-------|--------|--------|
| 0: Build Fix | #132 | 2-3d | All |
| 1A: Legacy Cleanup | legacy-removal | 1-2d | 1B |
| 1B: Typed Objects | #120 | 3-5d | Phase 2 |
| 2A: void* Elimination | void-star | 1-2d | Release |
| 2B: DECFLOAT/INT128 | features | 1-2d | Release |
| 2C: PDO Batch | features | 1d | Release |
| 3: Release | - | 1d | - |
| **Total** | | **10-16d** | |

## Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| Metadata struct discovery wrong | Phase 0 blocked | Read fbird_metadata.c FIRST |
| void* elimination scope creep | Phase 2 delayed | Wrapper header approach limits blast radius |
| Typed objects break PDO | Phase 1 regression | Test PDO early in 1B |
| FB 4.0 SKIPIF gaps | Tests fail on FB 3.0 | Check FB version in all new tests |

## Branch Consolidation (2026-03-27)

All v10.0.0 development branches consolidated into single `dev/v10` branch.

**Merged branches:**
1. `v10.0.0-modernization` - VERSION bump to 10.0.0-dev (clean merge)
2. `fix/v10-p0-legacy-handle-events` - build fixes, utils rewrite (clean merge)
3. `origin/feat/fix-transaction-class-conflict` - transaction class conflict fix (1 conflict in `src/cpp/fb_connection.hpp`, resolved via `--theirs`)
4. `origin/fix/ci-composer-install` - CI composer install fix (clean merge, auto-merged `.github/workflows/coverage.yml`)

**Test results:** BUILD FAILURE - extension did not compile. No tests ran.
- PASS: 0, FAIL: 0, SKIP: 0
- Root cause: C compilation errors in `fbird_result.c`, `fbird_blobs.c`, `fbird_query_exec.c` - API signature mismatches between v10 modernized headers and legacy C call sites (e.g., `_php_fbird_error()` missing required args, `fbb_free` implicit declaration, `FBIRD_VALIDATE_QUERY_EX` macro arity mismatch)
- Full report: `docs/plans/v10-test-report.txt`

**Status:** `dev/v10` branch created locally. Not pushed. Build failures are expected - these are pre-existing issues from the modernization work that need to be resolved in subsequent development.
