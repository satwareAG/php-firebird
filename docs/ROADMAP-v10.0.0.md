# ROADMAP v10.0.0 — The Final Modernization

**Created:** 2026-03-24
**Milestone:** [v10.0.0](https://github.com/satwareAG/php-firebird/milestone/7)
**Baseline:** v9.0.0 — 247/247 tests passing (100%)

## Overview

v10.0.0 marks the final transition of the `php-firebird` extension into a fully object-oriented
internal architecture. The primary goal is the elimination of PHP resource-based handles
in favor of typed C++ objects, and the complete removal of any remaining legacy `isc_*`
patterns from the codebase.

---

## 1. Typed Connection Objects — Issue #120 (L)

The core procedural API will be updated to return and accept `Firebird\Connection` objects instead of "Firebird link" resources.

- [ ] **1.1** Update `fbird_connect()` and `fbird_pconnect()` to return `Firebird\Connection` objects.
- [ ] **1.2** Implement object-to-resource compatibility layer (to prevent breaking every existing script).
- [ ] **1.3** Update all `fbird_*` procedural functions to accept both resources (deprecated) and objects.
- [ ] **1.4** Port all remaining resource types (Transaction, Result, Query, Blob) to typed objects.

## 2. Internal void* Elimination (L)

The internal C API defined in `firebird_utils.h` currently uses `void*` for opaque handles. This will be replaced with typed opaque structs for compile-time safety.

- [ ] **2.1** Define typed opaque structs for Connection, Transaction, Statement, Blob, etc.
- [ ] **2.2** Update all function signatures in `firebird_utils.h` and `firebird_utils.cpp`.
- [ ] **2.3** Update all call sites in `fbird_*.c` and `pdo_fbird/*.c`.

## 3. Legacy Pattern Removal (M)

- [ ] **3.1** Remove `legacy_handle_` and the `fbc_get_legacy_handle_ptr()` bridge from `fb::Connection`.
- [ ] **3.2** Modernize `fbird_events.c` to use the OO API `IEvents` interface directly.
- [ ] **3.3** Modernize `fbird_transaction.c` multi-db transactions (remove `ISC_TEB`/`isc_start_multiple`).

## 4. Feature Enhancements (M)

- [ ] **4.1** Implement Batch DML support in PDO driver (FB 4.0+).
- [ ] **4.2** Add native support for FB 4+ `DECFLOAT` and `INT128` types in Layer 2 OOP classes.

---

## Milestone Checklist

| Feature | Section | Status |
|---|---|---|
| Typed connection objects | §1 | ☐ |
| Internal void* elimination | §2 | ☐ |
| Legacy pattern removal | §3 | ☐ |
| PDO Batch DML | §4 | ☐ |
