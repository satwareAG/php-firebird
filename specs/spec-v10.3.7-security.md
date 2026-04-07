---
description: >-
  v10.3.7 security fixes: SQL injection in fbird_create_database, SPB buffer
  overflow in Service::__construct, stack buffer hardening, stale docs, config.m4 version.
tags: [security, buffer-overflow, sql-injection, v10.3.7]
priority: 1
---

> **Status: RELEASED** - Shipped in v10.3.7 (commit 6c937a2), included in v10.6.2

# Spec: v10.3.7 Security Fixes

## Goal

Fix 2 critical security vulnerabilities and 3 correctness issues identified in
the March 2026 critical analysis (`docs/CRITICAL_ANALYSIS_2026.md`).

## Success Criteria

- [x] C1: No unescaped user input in `fbird_create_database()` SQL construction
- [x] C2: SPB buffer in `Firebird\Service::__construct()` validates input lengths
- [x] M10: `fbird_create_database()` uses dynamic allocation instead of `char[4096]`
- [x] H5: CONTRIBUTING.md references correct repo URL and PHP/FB versions
- [x] M7: `config.m4` rejects PHP < 8.2
- [x] All existing tests pass (no regression)
- [x] New tests for C1 and C2 edge cases

## Part 1: C1 - SQL Injection in `fbird_create_database()`

### Current State

`fbird_connection.c:598-615` interpolates user-supplied `database`, `username`,
`password`, and `charset` directly into a `CREATE DATABASE` SQL string via `snprintf`
without escaping single quotes.

### Fix

1. Add `_php_fbird_escape_single_quotes()` helper that doubles `'` to `''`
2. Apply to `database`, `username`, `password` before interpolation
3. Validate `charset` against allowlist of known Firebird character sets
4. Replace `char create_sql[4096]` with `spprintf()` dynamic allocation (also fixes M10)

### Affected Files

- `fbird_connection.c` - `fbird_create_database()` function
- `tests/` - new `create_database_injection.phpt`

## Part 2: C2 - SPB Buffer Overflow in Service::__construct()

### Current State

`fbird_classes.c:856-870` uses `char buf[256]` for SPB construction.
`memcpy(buf + buf_len, user, user_len)` overflows when `user_len + pass_len > ~248`.
`(char)user_len` truncates lengths > 255.

### Fix

1. Validate `user_len <= 255` and `pass_len <= 255` (Firebird SPB protocol limit)
2. Validate total `buf_len + user_len + pass_len + 6 <= sizeof(buf)`
3. Throw `Firebird\ServiceException` on overflow instead of silent corruption
4. Also validate `host_len` for `loc[256]` buffer

### Affected Files

- `fbird_classes.c` - `FirebirdService::__construct()`
- `tests/` - new `oo_service_long_credentials.phpt`

## Part 3: H5 - CONTRIBUTING.md Stale Information

### Fix

| Line | Change |
|------|--------|
| 3, 8 | "PHP 8.1+" to "PHP 8.2+" |
| 10 | Remove "2.5" from Firebird versions |
| 98 | `FirebirdSQL/php-firebird` to `satwareAG/php-firebird` |
| 300 | Remove PHP 8.1 and Firebird 2.5 from test environments |

### Affected Files

- `CONTRIBUTING.md`

## Part 4: M7 - config.m4 PHP Version Check

### Fix

Change `config.m4:27`:
```m4
AC_MSG_CHECKING([for minimum PHP version 8.2])
```
And update the comparison to reject `php_minor -lt 2` when `php_major -eq 8`.

### Affected Files

- `config.m4`

## Risks

| Risk | Mitigation |
|------|------------|
| Quote escaping breaks legitimate paths with `'` | Test with paths containing single quotes |
| SPB length rejection breaks existing users | 255-byte limit is Firebird protocol spec |
| config.m4 change breaks PHP 8.1 users | PHP 8.1 EOL was Nov 2024, intentional |

## Test Strategy

1. `create_database_injection.phpt` - database path with `'`, verify no injection
2. `oo_service_long_credentials.phpt` - username > 255 bytes, verify exception
3. All 274 existing tests pass
4. Run with ASAN to verify no buffer overflows
