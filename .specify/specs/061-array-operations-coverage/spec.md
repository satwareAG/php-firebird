# Spec: Array Operations Test Coverage

**Issue**: #61
**Branch**: `test/array-operations-coverage`
**Date**: 2026-03-02
**Status**: Approved — implement after PR #72 (#59) merged

---

## Intent

Increase `fbird_query_array.c` coverage from 35.9% to ≥80%.
Firebird ARRAY type support is a niche but tested feature.

---

## Background

| File | Current | Target |
|------|---------|--------|
| `fbird_query_array.c` | 35.9% | ≥80% |

**Key functions in fbird_query_array.c** (from grep):
- `_php_fbird_arr_create` — create array descriptor
- `_php_fbird_arr_read` — read array values from server
- `_php_fbird_arr_write` — write array values to server
- `_php_fbird_fetch_array` — fetch array by descriptor
- `ibase_array_from_zval` / `ibase_array_to_zval` — PHP↔Firebird array conversion

**Existing coverage**: `tests/007.phpt` covers basic 1D array. Gaps:
- Multi-dimensional arrays (2D, 3D)
- NULL array elements
- Numeric subscript bounds (slice operations)
- Error paths: invalid array descriptor, mismatched dimensions
- All supported element types (INT, VARCHAR, DATE, etc.)

---

## Test Files to Create

| File | Tests |
|------|-------|
| `tests/coverage/array_operations_advanced.phpt` | 2D/3D arrays, bounds checking, element types |
| `tests/coverage/array_error_paths.phpt` | Invalid descriptor, dimension mismatch, NULL write |

---

## Key Notes

- Firebird ARRAY type requires `DECLARE col_name data_type[dim1, dim2]` DDL
- Arrays are fetched as PHP arrays; writing requires consistent dimensionality
- Must clean up test tables in `--CLEAN--` section

---

## Dependencies

- Requires: PR #72 (#59) merged
- No FB version constraint (ARRAY supported in Firebird 3.0+)

---

> Full plan.md + tasks.md to be created when branch is opened.
