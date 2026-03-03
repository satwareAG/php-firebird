# Spec: Batch Operations Test Coverage

**Issue**: #60
**Branch**: `test/batch-operations-coverage`
**Date**: 2026-03-02
**Status**: Approved — implement after PR #72 (#59) merged

---

## Intent

Achieve ≥80% line coverage on `firebird_utils.cpp` (Batch API) and the
`fbird_batch_*` entry points in `firebird.c`. These functions are only available
on Firebird 4.0+ and require `#if FB_API_VER >= 40` guards in tests.

---

## Background

| File | Estimated Coverage | Target |
|------|--------------------|--------|
| `firebird_utils.cpp` | <30% (complex C++ OO wrapper) | ≥80% |
| Batch functions in `firebird.c` | ~40% | ≥80% |

**Functions to cover** (already have some tests in `fbird_batch_*.phpt`):
- `fbird_batch_create` — basic batch creation
- `fbird_batch_add` — add record to batch
- `fbird_batch_execute` — execute batch
- `fbird_batch_cancel` — cancel in-progress batch
- `fbird_batch_blob_create` + `fbird_batch_blob_add` — blob batch operations
- Error paths for all batch functions (invalid handle, oversized messages)
- Multiple data types in a single batch (multitype)

---

## Test Files to Create

| File | Tests |
|------|-------|
| `tests/coverage/batch_error_paths.phpt` | Invalid handle, FB4+ skip guard, oversized message, null params |
| `tests/coverage/batch_advanced_types.phpt` | DATE/TIME/TIMESTAMP/DECIMAL types in batch, NULL values |
| `tests/coverage/batch_limits.phpt` | Max messages per execute, cancel mid-batch, create+immediate-cancel |

---

## Key Constraints

- ALL batch tests MUST have: `--SKIPIF--` checking `FB_API_VER >= 40`
  ```php
  $info = fbird_db_info($conn, FBIRD_STS_HDR_PAGES);
  // Check version from server info
  ```
- Or use the existing skip pattern from `tests/fbird_batch_001.phpt`
- `firebird_utils.cpp` coverage requires the C++ batch wrapper to be exercised
  (not just the PHP-level C function)

---

## Dependencies

- **Requires**: PR #72 (#59) merged (establishes testing patterns)
- Docker with Firebird 4.0+ image

---

## Constitution Alignment

| Article | Requirement |
|---------|-------------|
| I | C extension — test with .phpt |
| II | Tests write before CI, verify RED (batch error paths will PASS immediately since C exists) |
| VII | ≥80% but only for files touched by this PR |
| FB_API_VER | All batch tests skip on FB 3.0 |

---

> Full plan.md + tasks.md to be created when branch is opened.
