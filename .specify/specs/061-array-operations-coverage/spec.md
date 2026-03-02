# Spec: Array Operations Test Coverage

**Issue**: #61
**Branch**: `test/array-operations-coverage`
**Date**: 2026-03-02
**Status**: Implementing

---

## Intent

Achieve ≥80% line coverage on `fbird_query_array.c` (35.9% → 80%).
This file implements `_php_fbird_alloc_array()` and `_php_fbird_fetch_array()` — 
the internal functions that bridge PHP arrays to Firebird `SQL_ARRAY` columns.

---

## Background

| File | Estimated Coverage | Target |
|------|--------------------|--------|
| `fbird_query_array.c` | 35.9% | ≥80% |

**Key code paths to exercise:**
- `_php_fbird_alloc_array()` — array descriptor allocation, `isc_array_lookup_bounds`
- `_php_fbird_fetch_array()` — reading array values from Firebird into PHP arrays
- `fbird_array_create` / `fbird_array_set` — PHP userland array building
- Error paths: non-existent table/column, NULL IDs, garbage IDs, oversized arrays

---

## Test Files Created

| File | Tests |
|------|-------|
| `tests/coverage/array_operations_advanced.phpt` | 1D INTEGER roundtrip, VARCHAR array roundtrip, NULL array column, partial fill |
| `tests/coverage/array_error_paths.phpt` | non-existent table/col, non-array column, bind integer/string to array param, oversized PHP array |

---

## Constitution Alignment

| Article | Requirement |
|---------|-------------|
| I | C extension — `.phpt` tests |
| VII | ≥80% for `fbird_query_array.c` |
| No FB_API_VER guard | Arrays work on FB 3.0 and 4.0 |
