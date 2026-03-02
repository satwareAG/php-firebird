# Spec: Parameter Binding Test Coverage

**Issue**: #62
**Branch**: `test/parameter-binding-coverage`
**Date**: 2026-03-02
**Status**: Approved — implement after PR #72 (#59) merged

---

## Intent

Increase `fbird_query_bind.c` coverage from 41.8% to ≥80%.
Parameter binding is the most complex file (1165 lines) and handles all
SQL type conversions between PHP and Firebird.

---

## Background

| File | Current | Target |
|------|---------|--------|
| `fbird_query_bind.c` | 41.8% | ≥80% |

**Key internal functions** (all called via `fbird_execute`):
- `_php_fbird_bind_value` — top-level dispatcher
- `_php_fbird_bind_bool` — BOOLEAN type
- `_php_fbird_bind_int16/32/64` — integer types
- `_php_fbird_bind_float/double` — floating point
- `_php_fbird_bind_varchar` — string binding
- `_php_fbird_bind_blob` — BLOB binding
- `_php_fbird_bind_date/time/timestamp` — temporal types
- `_php_fbird_bind_decimal128` — FB4+ decimal128
- `_php_fbird_bind_null` — explicit NULL binding

**Existing coverage**: `tests/coverage/bind_edge_cases.phpt` covers INT64, empty string, large blob.
Gaps identified:
- BOOLEAN type (true/false/null)  
- DATE, TIME, TIMESTAMP with various PHP input formats
- FB4+ DECIMAL(38,x) and NUMERIC high precision
- Explicit NULL for each SQL column type
- Binding to non-existent parameter index
- String to numeric type coercion (PHP "123" → BIGINT)

---

## Test Files to Create

| File | Tests |
|------|-------|
| `tests/coverage/bind_boolean_null.phpt` | BOOLEAN true/false/NULL, explicit NULL for all types |
| `tests/coverage/bind_temporal_types.phpt` | DATE/TIME/TIMESTAMP: string format, object, unix timestamp |
| `tests/coverage/bind_numeric_types.phpt` | All INTEGER variants, FLOAT/DOUBLE precision edges, DECIMAL128 (FB4+) |
| `tests/coverage/bind_error_paths.phpt` | Too many params, wrong type coercion, missing params |

---

## Key Notes

- `_php_fbird_bind_decimal128` is `#if FB_API_VER >= 40` — needs skip guard
- DATE/TIME binding accepts: string `'YYYY-MM-DD'`, PHP DateTime object, unix int
- NULLable columns test binding `null` PHP value → Firebird NULL

---

## Dependencies

- Requires: PR #72 (#59) merged  
- `bind_numeric_types.phpt` partially requires FB4+ for DECIMAL128 section

---

> Full plan.md + tasks.md to be created when branch is opened.
