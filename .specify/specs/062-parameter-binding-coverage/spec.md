# Spec: Parameter Binding Test Coverage

**Issue**: #62
**Branch**: `test/parameter-binding-coverage`
**Date**: 2026-03-02
**Status**: Implementing

---

## Intent

Achieve ≥80% line coverage on `fbird_query_bind.c` (41.8% → 80%).
This file implements all parameter binding from PHP zval to Firebird XSQLDA/SQLVAR.

---

## Background

| File | Estimated Coverage | Target |
|------|--------------------|--------|
| `fbird_query_bind.c` | 41.8% | ≥80% |

**Existing coverage tests** (already on satware-main):
- `bind_boolean_invalid_string.phpt` — string "invalid" rejected for BOOLEAN
- `bind_boolean_must_be_boolean.phpt` — non-boolean types rejected
- `bind_boolean_numeric_string.phpt` — numeric string rejected for BOOLEAN
- `bind_edge_cases.phpt` — INT64 limits, empty string, large blob, explicit NULL (BIGINT/VARCHAR)
- `bind_long_scaled_out_of_range.phpt` — DECIMAL out of range (long scaled)
- `bind_short_scaled_out_of_range.phpt` — DECIMAL out of range (short scaled)

**Gaps addressed by new tests:**

| File | New Coverage |
|------|-------------|
| `bind_boolean_null.phpt` | Valid TRUE/FALSE/NULL roundtrip for BOOLEAN column |
| `bind_temporal_types.phpt` | DATE/TIME/TIMESTAMP: ISO string, unix int, NULL, epoch, future |
| `bind_numeric_types.phpt` | SMALLINT/INTEGER/BIGINT/FLOAT/DOUBLE/DECIMAL/NUMERIC zero/max/min/NULL |
| `bind_error_paths.phpt` | Too many/few params, false handle, wrong resource type, VARCHAR truncation, num_params/param_info errors |

---

## Constitution Alignment

| Article | Requirement |
|---------|-------------|
| I | C extension — `.phpt` tests |
| VII | ≥80% for `fbird_query_bind.c` |
