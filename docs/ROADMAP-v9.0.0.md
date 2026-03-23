# ROADMAP v9.0.0 — API Modernization

**Created:** 2026-03-23
**Milestone:** [v9.0.0](https://github.com/satwareAG/php-firebird/milestone/6)
**Baseline:** v8.3.0 — 241/241 tests passing (100%)

## Overview

v9.0.0 modernizes the procedural API surface: deprecating legacy patterns, adding
missing first-class functions, making exceptions the default error mode, standardizing
function signatures, adding stream-based BLOB binding, and (optionally) returning
typed connection objects.

---

## 1. Deprecate FBIRD_CREATE in fbird_query() — Issue #125 (S)

- [x] **1.1** Add `E_DEPRECATED` warning when `FBIRD_CREATE` is passed as first arg to `fbird_query()` — `fbird_query_exec.c` (S)
- [x] **1.2** Update stubs doc-comment for `fbird_query()` noting deprecation — `stubs/firebird-stubs.php` (S)
- [x] **1.3** Add test `tests/fbird_deprecate_create.phpt` — (S)

## 2. Add fbird_create_database() — Issue #121 (M)

- [x] **2.1** Extract DB creation logic from `fbird_query_exec.c` into `PHP_FUNCTION(fbird_create_database)` in `fbird_connection.c` — (M)
- [x] **2.2** Register `fbird_create_database` in function table — `firebird.c` (S)
- [x] **2.3** Add arginfo — `firebird.c` (S)
- [x] **2.4** Add stub — `stubs/firebird-stubs.php` (S)
- [x] **2.5** Add test `tests/fbird_create_database.phpt` — (S)

## 3. fbird_drop_db() with connection string — Issue #122 (S)

- [x] **3.1** Add overload: when first arg is string, treat as DSN, connect internally, drop, return — `fbird_connection.c` (M)
- [x] **3.2** Update arginfo and stubs — `firebird.c`, `stubs/firebird-stubs.php` (S)
- [x] **3.3** Add test `tests/fbird_drop_db_string.phpt` — (S)

## 4. Exception-by-default — Issue #123 (M)

- [x] **4.1** Change default `IBG(exception_mode)` from `FBIRD_EXCEPTION_MODE_SILENT` to `FBIRD_EXCEPTION_MODE_THROW` in GINIT — `firebird.c` (S)
- [x] **4.2** Also apply exception_mode in `_php_fbird_module_error()` (currently only checks INI) — `fbird_error.c` (S)
- [x] **4.3** Add `FBIRD_EXCEPTION_MODE_COMPAT` constant (alias for SILENT) for backward compat — `firebird.c` (S)
- [x] **4.4** Update stubs — `stubs/firebird-stubs.php` (S)
- [x] **4.5** Add test `tests/fbird_exception_default.phpt` — (S)

## 5. Standardize signatures — Issue #126 (M)

- [x] **5.1** Add `fbird_prepare_ex(resource $link, string $query, ?resource $trans = null)` with fixed signature — `fbird_query_prepare.c`, `firebird.c` (M)
- [x] **5.2** Add `fbird_trans_start()` documentation noting it as the preferred API — `stubs/firebird-stubs.php` (S)
- [x] **5.3** Add test `tests/fbird_prepare_ex.phpt` — (S)

## 6. BLOB stream params — Issue #129 (M)

- [x] **6.1** In `fbird_query_bind.c` SQL_BLOB case, detect `IS_RESOURCE` + `php_stream_from_res()`, read stream in chunks via `fbb_put_segment()` — (M)
- [x] **6.2** Add test `tests/fbird_blob_stream_param.phpt` — (S)

## 7. Typed connection object — Issue #120 (L)

- [ ] **7.1** Make `fbird_connect()` return `Firebird\Connection` object instead of resource — `fbird_connection.c`, `fbird_classes.c` (L)
- [ ] **7.2** Add backward-compat: object works as resource via `__toString`/casting — (M)
- [ ] **7.3** Update all internal `zend_fetch_resource` calls to also accept object — (L)
- [ ] **7.4** Add test `tests/fbird_typed_connection.phpt` — (S)

> **Note:** #120 is deferred — it requires touching every fbird_* function and is high-risk.
> Will be evaluated after all other v9.0.0 items are complete.

## 8. Deferred Deprecation Items (from DEPRECATION-AUDIT.md)

- [ ] **8.1** Replace `void*` opaque pointers with typed opaques in `firebird_utils.h` — (L)
- [ ] **8.2** Remove `ISC_TEB` usage, replace with OO API `fbt_start()` — (M)
- [ ] **8.3** Modernize `fbird_events.c` to use OO API events — (M)
- [ ] **8.4** Remove `legacy_handle_` from `fb::Connection` — (M)

> These are internal refactors with no user-facing API changes. Deferred to post-v9.0.0.

---

## Milestone Checklist

| Feature | Section | Status |
|---|---|---|
| Deprecate FBIRD_CREATE | §1 | ✅ |
| fbird_create_database() | §2 | ✅ |
| fbird_drop_db() string overload | §3 | ✅ |
| Exception-by-default | §4 | ✅ |
| Standardize signatures | §5 | ✅ |
| BLOB stream params | §6 | ✅ |
| Typed connection object | §7 | ☐ (deferred) |
| Deprecation audit items | §8 | ☐ (deferred) |
