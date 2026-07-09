---
description: >-
  v13.0.0 FB4+/FB5+ Feature Coverage: surface Firebird 4.0 and 5.0 server-side
  features that are currently SKIP'd or only partially implemented in the PHP
  driver. Each gap becomes a green test plus native implementation where missing.
  FB6+ features are deferred until an official Firebird 6.x Docker image exists.
tags: [fb4-plus, fb5-plus, stretch, v13.0, spec-sdd, milestone-index]
priority: 2
---

# Spec: v13.0.0 FB4+/FB5+ Feature Coverage

## Issue
#327 (this spec) - index for #417-#428, #435 (in scope) and #314, #429-#434 (deferred)

## Branch
`feat/v13-fb4-fb5-coverage`

## Date
2026-07-08

## Status
In progress — 10 of 13 issues done (#327, #418, #419, #420, #421, #423, #424, #425, #426, #427, #428). Remaining: #417 (DECFLOAT native type), #422 (full API), #435 (MARS).

---

## Intent

Close the Firebird 4.0 and 5.0 server-feature gaps surfaced as SKIP columns in
v12.1.0 M4 (`spec-v12.1-firebird-client-coverage.md`). Each gap becomes a green
test plus a native implementation where one is missing today.

This milestone is **spec-only** for issue #327. Implementation work happens in
separate `feat/*` branches per issue, each citing this spec.

### What changed since v12.1.0

v12.1.0 shipped with 4,668 test executions across 12 containers (PHP 8.2-8.5 x
FB 3.0/4.0/5.0), 0 failures. The FB4+/FB5+ methods were verified reachable but
skipped via `--SKIPIF--` because the underlying server features were not
exercised. v13.0.0 turns those skips into passing tests.

---

## Downstream Priority

| Downstream | FB Support Need | Status |
|------------|-----------------|--------|
| **amicron-platform** | FB 3.0 full | Done (v12.1.0) |
| **doctrine-firebird-driver** | Full FB (3.0/4.0/5.0+) | v13.0.0 enables FB4+/5+ |

doctrine-firebird-driver already maps FB4+ types in `Firebird4Platform.php`
but currently **coerces** them to legacy PHP types:

| FB4+ Type | Doctrine Mapping (current) | v13.0.0 enables |
|-----------|-----------------------------|------------------|
| `TIMESTAMP WITH TIME ZONE` | `Types::DATETIMETZ_MUTABLE` | Native TZ-aware handling (#419) |
| `TIME WITH TIME ZONE` | `Types::TIME_MUTABLE` | Native TZ-aware handling (#419) |
| `INT128` | `Types::BIGINT` (coerced) | Native precision (#418) |
| `DECFLOAT(16/34)` | `Types::DECIMAL` (coerced) | Native precision (#417) |

Priority within v13.0.0 follows doctrine's downstream needs:

1. **#419** TIME/TIMESTAMP WITH TIME ZONE — doctrine already has format strings + type declarations; needs driver end-to-end
2. **#417** DECFLOAT + **#418** INT128 — currently coerced; native enables precision-preserving operations
3. **#420** SET BIND — alternative type coercion path for doctrine
4. **#426** Scrollable cursors — pagination (LIMIT/OFFSET emulation)
5. **#421** Batch DML — doctrine has `createBatch()`/`executeBatch()` already
6. **#422** Statement timeout — connection pooling
7. **#425** READ CONSISTENCY — transaction isolation

---

## Version Gating Strategy

### Compile-time

Existing `FB_API_VER` preprocessor gates remain the canonical compile-time
mechanism (28+ sites already use `#if FB_API_VER >= 40` / `>= 50`). No new
named feature macros are introduced by this spec.

### Runtime SKIPIF

Three sanctioned patterns, in order of preference:

| Preference | Pattern | When to use | Why |
|------------|---------|-------------|-----|
| 1 (preferred) | **Capability probe** — `CREATE TABLE`/`EXECUTE BLOCK` probe-and-cleanup | New feature tests (DECFLOAT, INT128, scrollable cursors, etc.) | Self-documenting (SKIPIF shows exactly which feature is required); robust against config-disabled features and DataTypeCompatibility modes that version detection cannot catch |
| 2 (acceptable) | `get_fb_version()` from `tests/firebird.inc:168-183` | Version-gated tests where capability probing is impractical (e.g. batch API, timeouts) | Uses Service API (`fbird_server_info` + `FBIRD_SVC_SERVER_VERSION`) to query **real server version** — NOT the unreliable `fbc_get_server_version()` |
| 3 (acceptable) | `is_fb_server_available()` from `tests/cross_version.inc` | Cross-version tests targeting specific server versions | Docker-compose service probing + `rdb$get_context('SYSTEM','ENGINE_VERSION')` verification |

**NOT sanctioned**: `floatval(PDO::ATTR_SERVER_VERSION) < 4.0` — `PDO::ATTR_SERVER_VERSION` uses `fbc_get_server_version()` (`firebird_utils.cpp:744`) which returns the **client** library version, not the connected server version (tracked by #361, Unscheduled milestone).

Canonical capability-probe pattern (from `tests/pdo_fbird_fb4_datatypes_params.phpt:8-14`):

```php
--SKIPIF--
<?php
require_once __DIR__ . '/fb_version_probe.inc';
if (!fb_server_supports('DECFLOAT')) die('skip DECFLOAT not supported');
?>
```

### Shared helper recommendation

Implementations SHOULD add a shared probe helper
(`tests/fb_version_probe.inc`) wrapping the probe-and-cleanup pattern:

```php
/**
 * Probes whether the connected server supports a given feature.
 * Uses capability probing (CREATE TABLE / EXECUTE BLOCK) - never version strings.
 */
function fb_server_supports(string $feature): bool
```

Supported feature keys: `DECFLOAT`, `INT128`, `TIME_TZ`, `TIMESTAMP_TZ`,
`SET_BIND`, `BATCH_DML`, `STATEMENT_TIMEOUT`, `PACKAGES`, `SQL_SECURITY`,
`READ_CONSISTENCY`, `SCROLLABLE_CURSORS`, `PARALLEL_WORKERS`, `PROFILER`.

This is a **recommendation**, not a spec deliverable. Each implementation
issue may adopt the helper, use `get_fb_version()`, or inline its own probe
citing this spec.

---

## FB 4.0 Coverage Table

| Issue | Feature | Current State | Target |
|-------|---------|---------------|--------|
| #417 | DECFLOAT(16/34) native type | String converters only (`firebird_utils.cpp:1178-1215`, `IUtil->getDecFloat16/34->toString()`). No native PHP type, no round-trip (string->FB_DEC16/34). | Native PHP type, arithmetic precision preserved, SET BIND rules |
| #418 | INT128 native type | String converter only (`firebird_utils.cpp:1157-1175`, `IUtil->getInt128->toString()`). No string->FB_I128 encoder. | Native or string return (no precision loss), SET BIND rules |
| #419 | TIME/TIMESTAMP WITH TIME ZONE | Decode/encode helpers exist (`firebird_utils.cpp:1057-1243`, `IUtil->decodeTimeTz/decodeTimeStampTz/encodeTimeTz/encodeTimeStampTz`). No end-to-end test, no SET BIND coverage. | TZ-aware types handled correctly end-to-end, SET BIND works |
| #420 | SET BIND rule coverage (all rules) | Only `DECFLOAT TO VARCHAR` tested via PDO attr (`tests/pdo_fbird_bind_config.phpt`). `INT128 TO BIGINT` and `TIME ZONE TO LEGACY` tested via raw SQL only (`tests/cross_version/data_type_compat_*.phpt`). PDO attr path: `pdo_fbird.c:63`, `FBIRD_ATTR_SET_BIND=1011`. | All rules tested via PDO attr + SQL, LEGACY mode verified |
| #421 | Batch DML API (IBatch) full coverage | All 10 `fbird_batch_*` functions implemented (`fbird_batch.c`, gated `#if FB_API_VER >= 40` at line 20). OOP `Firebird\BatchHandle` class exists (`fbird_class_batch.c`). 13 test files exist covering all 10 functions (create:17, add:10, execute:8, cancel:7, add_blob:4, add_blob_stream:2, set_default_bpb:2, register_blob:1, get_blob_alignment:1, append_blob_data:1). 3 undertested functions need edge-case coverage: `register_blob`, `get_blob_alignment`, `append_blob_data`. | All 10 functions have robust edge-case coverage, multi-type batch works |
| #422 | Statement + session idle timeout | Zero test coverage. API: `IAttachment::getStatementTimeout/setStatementTimeout/getIdleTimeout/setIdleTimeout` + `IStatement::getTimeout/setTimeout`. `fb::VersionInfo::hasTimeouts()` returns `version_ >= FB40`. | Timeouts enforce correctly (ms for statement, seconds for session) |
| #423 | Packages + SQL SECURITY {DEFINER/INVOKER} | Zero test coverage. SQL: `CREATE PACKAGE/BODY`, `ALTER DATABASE SET DEFAULT SQL SECURITY`. | Package create/exec works, SQL SECURITY enforced |
| #424 | EXECUTE STATEMENT rich form + SET/AT TIME ZONE | Zero test coverage. SQL: `SET TIME ZONE <tz|LOCAL>`, `AT TIME ZONE` operator, `EXTRACT(TIMEZONE_HOUR|MINUTE|NAME FROM ...)`. | TZ SQL works end-to-end |
| #425 | READ CONSISTENCY transaction isolation | Zero test coverage. TPB: `isc_tpb_read_consistency=22`. Statement-level snapshot, auto-restart on conflict. Constant: `FBIRD_TXN_READ_CONSISTENCY`. | Read consistency works, auto-restart on update conflict verified |

### Batch DML function inventory (#421)

All 10 functions exist in `stubs/firebird-stubs.php` and `fbird_batch.c`:

| # | Function | Stub line | C impl line |
|---|----------|-----------|-------------|
| 1 | `fbird_batch_create` | 1202 | 93 |
| 2 | `fbird_batch_add` | 1212 | 586 |
| 3 | `fbird_batch_add_blob` | 1223 | 686 |
| 4 | `fbird_batch_register_blob` | 1233 | 719 |
| 5 | `fbird_batch_execute` | 1242 | 606 |
| 6 | `fbird_batch_cancel` | 1251 | 657 |
| 7 | `fbird_batch_get_blob_alignment` | 1262 | 760 |
| 8 | `fbird_batch_append_blob_data` | 1272 | 788 |
| 9 | `fbird_batch_add_blob_stream` | 1282 | 817 |
| 10 | `fbird_batch_set_default_bpb` | 1292 | 846 |

---

## FB 5.0 Coverage Table

| Issue | Feature | Current State | Target |
|-------|---------|---------------|--------|
| #426 | Scrollable cursors (6 orientations) | **Fully implemented** in `src/cpp/fb_statement.hpp:332-424` (`fetchPrior/fetchFirst/fetchLast/fetchAbsolute/fetchRelative`) and C wrappers in `firebird_utils.cpp:1392-1425`. `fb::VersionInfo::hasBlobCaching()` exists but no `hasScrollableCursors()`. Existing tests: `tests/pdo_fbird_scrollable_cursor.phpt`, `tests/pdo_fbird/conformance/pdo_definition_fetch_orientation.phpt`. Needs full 6-orientation coverage + BOF/EOF detection. | All 6 orientations tested, BOF/EOF detection verified |
| #427 | Parallel workers | Zero test coverage. DPB: `isc_dpb_parallel_workers=100`. Parallel sweep + index creation. `fb::VersionInfo::hasParallelExec()` returns `version_ >= FB50`. | Parallel workers configurable, parallel sweep verified |
| #428 | Profiler plugin | Zero test coverage. API: `IProfilerPlugin/startSession`. `fb::VersionInfo::hasProfiler()` returns `version_ >= FB50`. | Profiler session started, performance counters collected |

---

## Cross-Cutting

| Issue | Feature | Current State | Target |
|-------|---------|---------------|--------|
| #435 | PDO multiple active result sets (MARS) | Listed as "Not yet implemented" in `stubs/pdo-fbird-stubs.php:197-201`. Architectural: requires cursor pool per connection. Currently second `prepare()` closes first cursor. Original spec ref: #322 (PDO conformance). | Multiple `PDOStatement` objects active simultaneously on single `PDO` connection |

---

## Current Implementation State

Features with partial implementations to build on (no reinvention needed):

| Feature | What exists | What is missing |
|---------|-------------|-----------------|
| DECFLOAT (FB4+) | `fbu_decfloat16_to_string` / `fbu_decfloat34_to_string` via `IUtil->getDecFloat16/34->toString()` (`firebird_utils.cpp:1178-1215`, gated `#if FB_API_VER >= 40`) | Native PHP type, round-trip encoder (string->FB_DEC16/34) |
| INT128 (FB4+) | `fbu_int128_to_string` via `IUtil->getInt128->toString()` (`firebird_utils.cpp:1157-1175`) | String->FB_I128 encoder, native PHP handling |
| TimeTz / TimeStampTz (FB4+) | `fbu_decode_time_tz` / `fbu_decode_timestamp_tz` / `fbu_encode_time_tz` / `fbu_encode_timestamp_tz` via `IUtil` (`firebird_utils.cpp:1057-1243`) | End-to-end tests, SET BIND coverage |
| Scrollable cursors (FB5+) | Full impl in `fb_statement.hpp:332-424` + C wrappers `fbs_fetch_prior/first/last/absolute/relative()` (`firebird_utils.cpp:1392-1425`). BOF/EOF fix in `pdo_fbird_stmt.c` (cursor stays open). Edge-case tests in `pdo_fbird_scrollable_cursor_edge_cases.phpt` | **Done** |
| Batch API (FB4+) | All 10 `fbird_batch_*` funcs + `Firebird\BatchHandle` OOP class. 13 test files exist (all 10 funcs covered). 3 undertested: `register_blob`, `get_blob_alignment`, `append_blob_data` (1 test each) | Edge-case coverage for 3 undertested functions |
| SET BIND (FB4+) | PDO attr path (`pdo_fbird.c:63`, `FBIRD_ATTR_SET_BIND=1011`) + SQL path (`fb_connection.hpp:64,368`) | Only 1 of 3 rules tested via PDO attr; LEGACY mode untested |

Features with **zero** implementation (greenfield):
- Statement/session idle timeout tests (#422)
- Packages + SQL SECURITY tests (#423)
- EXECUTE STATEMENT rich form + SET/AT TIME ZONE tests (#424)
- READ CONSISTENCY tests (#425)
- Parallel workers tests (#427)
- Profiler plugin tests (#428)
- PDO MARS (#435 - architectural)

---

## Out of Scope / Future

### FB 6.0 features (deferred)

FB 6.0 issues are deferred until Firebird releases an official 6.x Docker image.
Issue #314 (FB6 Docker service) tracks the blocker. When resolved, a separate
spec (`spec-v13.1-fb6-coverage.md` or similar) will cover:

| Issue | Feature |
|-------|---------|
| #314 | ci: add firebird60 Docker service (FB 6.0) |
| #429 | FB6 schemas (`IMessageMetadata::getSchema` / `IMetadataBuilder::setSchema`) |
| #430 | FB6 inline blob transfer (`IAttachment::getMaxBlobCacheSize/setMaxBlobCacheSize/getMaxInlineBlobSize/setMaxInlineBlobSize`) |
| #431 | FB6 range-based FOR loop + `GENERATE_SERIES()` + table-valued functions |
| #432 | FB6 named/default procedure arguments |
| #433 | FB6 `WITHIN GROUP ORDER BY` + custom aggregate UDR (`IExternalAggregateFunction`) |
| #434 | FB6 `IUtil::executeCreateDatabase2` with DPB |

### Other out-of-scope items

- `fbird_server_version` at connection level (#361, Unscheduled milestone) - capability-probe SKIPIF makes it unnecessary for v13.0.0
- v12.1.0 M2 procedural backlog (#436-#441, Unscheduled milestone)
- FB 2.5 legacy SQL features
- Performance benchmarking across FB versions

---

## Issue Index

### In scope (13 issues)

| Issue | Title | Status |
|-------|-------|--------|
| #327 | spec: write this file (this spec) | **Done** |
| #419 | feat: FB4 TIME/TIMESTAMP WITH TIME ZONE | **Done** (P1) |
| #417 | feat: FB4 DECFLOAT(16/34) native type support | Test coverage done — native type TODO (P2) |
| #418 | feat: FB4 INT128 native type support | **Done** (P2) |
| #420 | feat: FB4 SET BIND rule coverage (all rules) | **Done** (P3) |
| #426 | feat: FB5 scrollable cursors (6 orientations) | **Done** (P4) |
| #421 | feat: FB4 batch DML API (IBatch) full coverage | **Done** (P5) |
| #422 | feat: FB4 statement + session idle timeout | SQL-only done — full API TODO (P6) |
| #425 | feat: FB4 READ CONSISTENCY transaction isolation | **Done** (P7) |
| #423 | feat: FB4 packages + SQL SECURITY {DEFINER/INVOKER} | **Done** |
| #424 | feat: FB4 EXECUTE STATEMENT rich form + SET TIME ZONE + AT TIME ZONE | **Done** |
| #427 | feat: FB5 parallel workers | **Done** |
| #428 | feat: FB5 profiler plugin | **Done** |
| #435 | feat: PDO multiple active result sets | Open |

### Deferred (7 issues - blocked on FB6 Docker image)

| Issue | Title | Status |
|-------|-------|--------|
| #314 | ci: add firebird60 Docker service | Open |
| #429 | feat: FB6 schemas | Open |
| #430 | feat: FB6 inline blob transfer | Open |
| #431 | feat: FB6 range-based FOR loop + GENERATE_SERIES() + TVFs | Open |
| #432 | feat: FB6 named/default procedure arguments | Open |
| #433 | feat: FB6 WITHIN GROUP ORDER BY + custom aggregate UDR | Open |
| #434 | feat: FB6 IUtil::executeCreateDatabase2 with DPB | Open |
