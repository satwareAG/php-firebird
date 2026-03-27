---
description: >-
  v10.0.0 feature enhancements: PDO batch DML, DECFLOAT/INT128
  native type support, modern Firebird 4.x type handling.
tags: [v10, features, pdo, decfloat, int128]
priority: 5
---

# Spec: v10 Feature Enhancements

## Goal

Add missing Firebird 4.x type support and PDO driver enhancements that users
have requested since v8.

## Success Criteria

- [ ] DECFLOAT (16/34) types return as `string` (no precision loss)
- [ ] INT128 types return as `string` (PHP has no native int128)
- [ ] PDO batch DML: `pdo_fbird` supports `exec()` with multiple statements
- [ ] `fbird_field_info()` reports DECFLOAT and INT128 subtypes correctly
- [ ] Stubs updated for all new type handling
- [ ] Tests for DECFLOAT, INT128, and PDO batch DML

## Part 1: DECFLOAT/INT128 Types

### Current State

- Firebird 4.0+ supports `DECFLOAT(16)` and `DECFLOAT(34)` SQL types
- Firebird 4.0+ supports `INT128` SQL type
- Extension treats both as unknown blobs or returns corrupted values
- Existing test `datatype_int128.phpt` exists but may be incomplete

### Target State

| SQL Type | C Mapping | PHP Return | Notes |
|----------|-----------|------------|-------|
| `DECFLOAT(16)` | `blr_dec_float` | `string` | Full precision, no float conversion |
| `DECFLOAT(34)` | `blr_dec64` | `string` | Full precision |
| `INT128` | `blr_int128` | `string` | PHP int overflows at 64-bit |

Implementation in `fbird_result.c`:
- Detect `blr_dec_float`, `blr_dec64`, `blr_int128` in result column parsing
- Convert to string via `sprintf` with appropriate format
- No `DECIMAL`/`NUMERIC` change (those already work via `fbird_query_bind.c`)

### Affected Files

- `fbird_result.c` - add type detection for DECFLOAT/INT128
- `fbird_metadata.c` - `fbird_field_info()` subtype mapping
- `stubs/firebird-stubs.php` - document return type
- `phpstan/fbird.stub.php` - update field_info docs

## Part 2: PDO Batch DML

### Current State

- `PDO::exec()` in `pdo_fbird_driver.c` executes single statement only
- Users must call `exec()` in a loop for batch operations
- No native batch API exposed through PDO

### Target State

```php
// Multi-statement exec (semicolon-separated):
$affected = $pdo->exec("INSERT INTO t1 VALUES (1); INSERT INTO t1 VALUES (2);");

// Batch prepare + execute (existing fbird_batch_* not exposed via PDO):
// Defer to v10.1 - too much scope for v10.0
```

Scope for v10.0: Split `PDO::exec()` string on `;`, execute each, sum affected rows.
Error on any failure (rollback semantics).

### Affected Files

- `pdo_fbird/pdo_fbird_driver.c` - `fbird_pdo_exec()` multi-statement split
- `stubs/pdo-fbird-stubs.php` - document multi-statement behavior
- `tests/` - new `pdo_batch_dml.phpt`

## Part 3: Modern Defaults (DONE in v9)

Per ROADMAP-v10.0.0.md, these are already complete:
- Firebird 4.3 modern defaults (wire encryption, time zone awareness)
- Connection parameter defaults updated

No action needed. Listed for completeness.

## Risks

| Risk | Mitigation |
|------|------------|
| DECFLOAT string format varies by locale | Use `sprintf("%.16g")` / `sprintf("%.34g")`, locale-independent |
| INT128 > PHP_INT_MAX | Always return string; document behavior |
| PDO batch DML SQL injection via split | Only split on `;` outside quotes (simple state machine) |
| FB 3.0 doesn't have DECFLOAT | SKIPIF check for FB version >= 4.0 |

## Test Strategy

1. `decfloat_001.phpt` - CREATE TABLE with DECFLOAT(16)/(34), INSERT, SELECT, verify string
2. `int128_002.phpt` - extend existing test with boundary values
3. `pdo_batch_dml.phpt` - multi-statement exec, verify affected row count
4. `pdo_batch_dml_error.phpt` - verify rollback on mid-batch failure
5. All existing tests pass (no regression)