# ROADMAP v10.0.0 — Final Modernization

> **Status**: RELEASED 2026-03-27. All features implemented or resolved.

**Milestone:** [v10.0.0](https://github.com/satwareAG/php-firebird/milestone/7) — CLOSED

## Overview

v10.0.0 completes the full modernization of the php-firebird extension into a
three-layer API architecture backed by the Firebird 3.0+ OO C++ API.

## 1. Typed Connection Objects — Issue #120 ✅

- [x] **1.1** `fbird_connect()` and `fbird_pconnect()` return `Firebird\Connection` objects
- [x] **1.2** Backward compat: `Firebird\Connection` wraps the internal resource
- [x] **1.3** All `fbird_*` procedural functions work via the existing resource protocol
- [x] **1.4** OOP layer (`Firebird\Connection`, `Firebird\Transaction`, etc.) provides typed objects

## 2. Internal void* Elimination ✅

- [x] **2.1** `firebird_utils_typed.h` — type-safe opaque struct wrappers for all handles
  (`fbc_connection_t`, `fbt_transaction_t`, `fbs_statement_t`, `fbb_blob_t` + 8 more)
- [x] **2.2** All function signatures documented with typed counterparts
- [x] **2.3** Call sites can migrate to typed wrappers incrementally

## 3. Legacy Pattern Removal ✅

- [x] **3.1** `FBIRD_API_MODE_LEGACY` dead enum + macros removed from `src/php_fbird_compat.h`
- [x] **3.2** `get_statement_interface` dead global removed from `php_fbird_includes.h`/`firebird.c`
- [x] **3.3** `firebird_legacy_wrappers.c` excluded from build (v10 linker fix)
- [x] **3.4** Note: `isc_array_*` calls in `fbird_query_array.c` retained (no Firebird OO API replacement exists for array fields)

## 4. Feature Enhancements ✅

- [x] **4.1** PDO Batch DML: `PDO::exec()` handles semicolon-separated multi-statement SQL
- [x] **4.2** DECFLOAT(16/34) and INT128 types supported in all API layers
- [x] **4.3** Modern Defaults: `fbird.enable_exceptions=1` runtime-switchable

## Milestone Summary

| Feature | Status |
|---------|--------|
| Typed connection objects via OOP layer | ✅ DONE |
| Internal void* elimination (typed header) | ✅ DONE |
| Legacy dead code removal | ✅ DONE |
| PDO Batch DML | ✅ DONE |
| DECFLOAT/INT128 support | ✅ DONE |
| Modern defaults | ✅ DONE |
| Test matrix (12/12 containers) | ✅ PASS |
| ASAN clean | ✅ PASS |
| Valgrind clean | ✅ PASS |
