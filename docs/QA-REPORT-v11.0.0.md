# php-firebird v11.0.0 — Quality Inspection Report

**Date**: 2026-04-09
**Audited commit**: `3088145` (`satware-main`)
**Audit method**: 5-agent parallel sub-swarm (425 tool calls, 5 dimensions)
**Status**: POST-RELEASE — v11.0.0 published 2026-04-09

---

## Executive Summary

| Dimension | Health | Critical | Warning | Info |
|-----------|--------|----------|---------|------|
| C Code Quality | 🟡 FAIR | 1 | 10 | 5 |
| PHP Stubs / PHPStan | 🟢 GOOD | 0 | 3 | 4 |
| Test Suite | 🟡 FAIR | 0 | 6 | 8 |
| Documentation | 🔴 POOR | 3 | 7 | 5 |
| CI/CD & Release | 🟡 FAIR | 0 | 8 | 3 |

**Overall: 4 Critical, 34 Warnings, 25 Info findings across the codebase.**

The CI/CD infrastructure is mature with 100% pinned actions. The stubs are perfectly synced (89/89 functions). The biggest gaps are: the README still advertises v10.6.2 (not v11.0.0), 69.5% of tests lack `--CLEAN--` sections, one functional regression in `fbird_delete_user`'s arginfo, and a supply-chain risk from a mutable third-party shell script in `codeql.yml`.

---

## Dimension 1 - C Code Quality

**Agent 1 findings from** `firebird.c`, `fbird_*.c`, `fbird_classes.c`, `fbird_service.c`, `firebird_utils.cpp`, all headers.

### CRITICAL

#### C1 - `arginfo_fbird_delete_user` functional regression
**File**: `firebird.c:340` vs `fbird_service.c:175`
The arginfo declares **3 required parameters** (`service_handle`, `user_name`, `password`) but the C implementation accepts only **2** (`"zs"` format for delete_user). Any call to `fbird_delete_user($svc, $username)` will raise "Too few arguments". This is a silent regression affecting user deletion.
**Fix**: Correct the arginfo to `ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_delete_user, 0, 2, _IS_BOOL, 0)` with 2 required params.

### WARNINGS

#### C2 - Verbose flag write before bounds check (off-by-one risk)
**File**: `fbird_service.c:521-527`
`buf[spb_len++] = isc_spb_verbose` executes BEFORE the `if (spb_len > sizeof(buf))` check. When `slprintf` fills the buffer to exactly `sizeof(buf)-1`, the NUL terminator is corrupted.
**Fix**: Move bounds check before the verbose byte write.

#### C3 - M3 migration incomplete for `fbird_batch_create`
**File**: `fbird_batch.c:180`
Uses `RETVAL_RES(zend_register_resource(...))` - the only remaining `RETVAL_RES` in the codebase. All other resource types (Connection, Transaction, ResultSet, Blob, Event, Service) use OOP wrappers. `fbird_setup_batch_object()` is defined in `fbird_classes.c` but never called from `fbird_batch.c`.

#### C4 - `fbird_classes_internal.h` is orphaned and stale
**File**: `fbird_classes_internal.h`
This header defines `fbird_connection_obj`, `fbird_transaction_obj` etc. with different layouts than the actual production structs in `fbird_classes.c`. It is **never included** by any `.c` file. A maintenance landmine - anyone including it gets wrong struct offsets.
**Fix**: Remove the file entirely, or update and actually use it.

#### C5 - Duplicate service struct definition
**Files**: `fbird_service.c:17` and `fbird_classes.c:868`
`fbird_service` and `fbird_service_rsrc` are layout-identical structs in separate files with different type names. `fbird_classes.c` casts raw pointers between the two, which works in practice but is a maintenance hazard.
**Fix**: Unify into a shared header.

#### C6 - `RETURN_FALSE` without error reporting (~4 sites)
Silent failures:
- `fbird_connection.c:624` - when `IBG(default_link) == NULL`
- `fbird_connection.c:630` - when `_php_fbird_res_from_zval()` returns NULL
- `fbird_transaction.c:323` - when `!ib_link`
- `fbird_service.c:329` - in `fbird_service_detach` resource resolution failure
All return `false` with no `php_error_docref()` or `FBIRD_SVC_ERROR()` call.

#### C7 - `zend_bool` deprecated type (13 usages)
**Files**: `fbird_connection.c`, `fbird_metadata.c`, `fbird_service.c`, `fbird_classes_internal.h`, `php_fbird_includes.h`
`zend_bool` was deprecated in PHP 8.0. Should use `bool` directly.

#### C8 - 163 untyped `ZEND_ARG_INFO` parameters in procedural API
**File**: `firebird.c`
All 89 procedural function arginfos use `ZEND_ARG_INFO(0, param)` without type annotations. Weakens reflection and prevents compile-time type validation.

#### C9 - 9 OOP method arginfos missing return types
**File**: `fbird_classes.c`
`arginfo_fbird_connection_beginTransaction`, `arginfo_fbird_resultset_fetch`, `arginfo_fbird_statement_execute`, `arginfo_fbird_connection_prepare`, `arginfo_fbird_blob_create`, `arginfo_fbird_blob_open`, `arginfo_fbird_blob_read` use bare `ZEND_BEGIN_ARG_INFO_EX` without `ZEND_BEGIN_ARG_WITH_RETURN_TYPE_*`.

#### C10 - `%ld` format for `zend_long` on Windows
**File**: `firebird.c:1055`
`snprintf(query, ..., "%ld", inc)` where `inc` is `zend_long`. On Windows 64-bit, `long` is 32-bit but `zend_long` is 64-bit. Should use `ZEND_LONG_FMT` or `"%" PRId64`.

#### C11 - `Firebird\Event` registered but has no methods (Phase H pending)
`fbird_classes.c:1394` registers `Firebird\Event` with `NULL` methods. The class is instantiated by `fbird_set_event_handler` but no methods are accessible from PHP. Documented as "Phase H" work.

### INFO (C)

- `PHP_FUNCTION(fbird_timefmt)` declared in `php_firebird.h:58` but never implemented/exported — dead declaration
- `sizeof(user_flags)` in `fbird_service.c:188` on `char[]` — accidental correctness (should use `sizeof/sizeof[0]`)
- `safe_emalloc(1-1, ...)` confusing pattern in `fbird_transaction.c:347` — evaluates to `emalloc(sizeof(fbird_transaction))`
- `call_user_function()` bridge for OOP `beginTransaction`/`prepare`/`execute` — function-name-lookup overhead per call
- No TODO/FIXME/HACK/XXX found anywhere - **CLEAN** ✅
- No `sprintf`/`strcpy`/`gets` - all string ops use `snprintf`/`slprintf`/`spprintf` - **CLEAN** ✅
- No raw `malloc` in request context - persistent connection use is correct - **CLEAN** ✅
- All 6 classes + 3 exceptions registered in `fbird_register_classes()` - **COMPLETE** ✅

---

## Dimension 2 - PHP Stubs & PHPStan

**Agent 2 findings from** `stubs/`, `phpstan/`, `phpstan.neon`, `phpcs.xml`, `scripts/check-stubs-sync.sh`.

### PASS checks

| Check | Result |
|-------|--------|
| `bash scripts/check-stubs-sync.sh` | ✅ 89/89 functions, perfect sync |
| `stubs/composer.json` branch-alias | ✅ `11.0.x-dev` |
| PHP_FE cross-check (all 89 functions) | ✅ All covered in both stub files |
| Phantom function stubs | ✅ None (fbird_timefmt correctly absent) |
| @version tags in published stubs | ✅ All say 11.0.0 |
| phpstan.neon level | ✅ Level 8 |
| phpcs.xml PSR-12 120-char | ✅ PSR-12 inherits 120-char default |
| firebird-classes.php OOP classes | ✅ 10 classes (required 6) |
| pdo-fbird-stubs.php vs C implementation | ✅ All 28 PDO constants match |

### WARNINGS

#### S1 - `FBIRD_VER` constant wrong value in stubs
| Location | Value | Correct |
|----------|-------|---------|
| `php_firebird.h` (runtime) | `#define PHP_FIREBIRD_VER 100` | ✅ |
| `stubs/firebird-stubs.php` | `const FBIRD_VER = 90` | ❌ stale v9.x |
| `phpstan/fbird.stub.php` | `const FBIRD_VER = 10` | ❌ wrong (should be 100) |
| `phpstan/fbird-bootstrap.php` | `define('FBIRD_VER', 10)` | ❌ wrong (should be 100) |

**Fix**: Set to `100` in all three locations.

#### S2 - `FBIRD_EXCEPTION_MODE_COMPAT` missing from stubs
`firebird.c:868` registers `FBIRD_EXCEPTION_MODE_COMPAT` (alias for `FBIRD_EXCEPTION_MODE_SILENT = 0`) but it is absent from `stubs/firebird-stubs.php` and `phpstan/fbird.stub.php`. Static analysis consumers cannot see this constant.

#### S3 - `phpstan/fbird-bootstrap.php` missing constants
Missing from the bootstrap (but present in stubs):
- `FBIRD_EXCEPTION_MODE_SILENT`, `FBIRD_EXCEPTION_MODE_THROW`, `FBIRD_FETCH_DATE_OBJ`
- All `FBIRD_BKP_*`, `FBIRD_RES_*`, `FBIRD_PRP_*`, `FBIRD_RPR_*`, `FBIRD_STS_*`, `FBIRD_SVC_*` service constants

#### S4 - `PDO_FBIRD_ATTR_PAGE_BUFFERS` registered but never handled
Constant 1004 is registered in `pdo_fbird.c` and stubbed correctly, but the `setAttribute`/`getAttribute` switch in `pdo_fbird_driver.c` has no case for it. The attribute silently does nothing at runtime.

### INFO (Stubs)

- `phpstan.neon phpVersion: 80100` vs `composer.json require.php >=8.2` minor mismatch — PHPStan won't flag 8.2-only features
- `stubs/README.md` lists only 2 classes; file now contains 10 classes
- `phpstan/firebird-event.stub.php` is intentionally empty (content moved to firebird-classes.php) but still listed in `phpstan.neon` — could be removed
- `fbird_prepare`, `fbird_prepare_ex`, `fbird_batch_create`, `fbird_reconnect_transaction` return `mixed` — known limitation, no named OOP class for prepared statements yet

---

## Dimension 3 - Test Suite

**Agent 3 findings from** 236 `.phpt` tests in `tests/`, 40 in `tests/coverage/`, fuzz infrastructure.

### Summary Metrics

| Metric | Value | Target |
|--------|-------|--------|
| Total phpt files | 277 (236 + 40 coverage) | - |
| --SKIPIF-- coverage | 234/236 = **99.2%** | 100% |
| --CLEAN-- coverage | 72/236 = **30.5%** | >80% |
| OOP API (`Firebird\*`) tests | 31/236 = **13.1%** | >50% |
| Dual-accept bridge tests | 4/236 = **1.7%** | >10% |
| Legacy `is_resource()` only | 12 files | 0 |

### WARNINGS

#### T1 - 69.5% of tests missing --CLEAN-- (state leak risk)
164/236 tests lack `--CLEAN--` sections. High-risk files that CREATE tables without cleanup:
- `migration_001.phpt` - creates `TEST_MIGRATION_FORCE` table
- `datatype_001.phpt` - creates `TEST_001` table
- `long_names_001/002.phpt` - create long-named tables
- All 7 service tests (none have CLEAN) including backup that creates `/tmp/` files

#### T2 - OOP API test coverage at only 13%
Only 31/236 tests use the new `Firebird\*` namespace classes. The v11.0 M3 migration is complete in C but barely tested in the OOP layer:
- `Firebird\Statement` - only 1 test file (`fbird_classes_004.phpt`)
- `Firebird\ResultSet` - 3 files total, limited scenarios
- `Firebird\Blob` - minimal; `blob_segfault_after_commit.phpt` still uses procedural-only

#### T3 - 12 test files still use `is_resource()` without OOP fallback
Files that should be updated to check `instanceof Firebird\*` or use dual-bridge pattern:
`blob_stream_chunked_write.phpt`, `execute_safety_001.phpt`, `fbird_batch_no_params_001.phpt`, `issue120.phpt`, `issue131.phpt`, `test_blob_stream.phpt`, plus 6 others

#### T4 - `event_poller_wrapper.phpt` tests PHP wrappers, not live Firebird events
Tests the `ProcessEventPoller`/`PcntlEventPoller`/`FiberEventPoller` strategy pattern in PHP userland, but does NOT test actual Firebird event registration/firing via `fbird_event_wait`. No live event trigger test exists anywhere.

#### T5 - RPR_MEND_DB teardown pattern not implemented
`fbird_service_db_mgr.phpt` calls `FBIRD_RPR_VALIDATE_DB` but never `FBIRD_RPR_MEND_DB`. Per `.clinerules`, validate must always be paired with mend teardown to avoid stale "damaged" flags on subsequent runs.

#### T6 - `tests/common.inc` has no OOP-aware helpers
All helper functions in `common.inc` use the procedural API. No `oop_connect()`, `oop_begin_transaction()` etc. exist, making it harder to write consistent OOP tests.

#### T7 - 39/40 coverage tests missing --CLEAN--
`tests/coverage/` subdirectory: 1/40 tests has `--CLEAN--`. While many use `RECREATE TABLE` inline, this is unreliable.

#### T8 - Service backup tests leave `/tmp/` files
`fbird_service_oo_002.phpt` does a backup/restore round-trip creating files in `/tmp/` but has no `--CLEAN--` section to remove them.

### INFO (Tests)

- SKIPIF coverage is excellent at 99.2% - the 2 missing are correctly non-DB tests ✅
- `tests/config.inc` hardcodes `masterkey` password — should fall back to `ISC_PASSWORD` env var
- `tests/config.inc` has no `FIREBIRD_PORT` support for non-standard ports
- Fuzz infrastructure is modular with SARIF output but tests only the procedural API — no OOP fuzzing
- `scripts/check_db_integrity.sh` has hardcoded Docker path `/firebird/data/test.fdb` and hardcoded credentials — unusable outside Docker

**Missing coverage areas**:
- `Firebird\Statement` multi-row fetch, re-execute, free
- Trigger creation/firing tests
- Concurrent transaction conflict scenarios
- Negative OOP testing (e.g. call `beginTransaction()` on closed connection)
- Connection pooling / persistent connection via OOP API

---

## Dimension 4 - Documentation

**Agent 4 findings from** README.md, CHANGELOG.md, NEXT_STEPS.md, docs/, specs/, plans/.

### CRITICAL

#### D1 - README version badge still shows 10.6.2
**File**: `README.md:6`
```markdown
[![Version](https://img.shields.io/badge/version-10.6.2-blue.svg)](CHANGELOG.md)
```
Must be updated to `version-11.0.0-blue.svg`.

#### D2 - README "Current Version" section and download URL reference v10.6.2
**Files**: `README.md:933` (section header), `README.md:962` (wget URL), `README.md:966` (tarball filename)
All three must be updated to v11.0.0.

#### D3 - Broken link in README.md
**File**: `README.md:929`
Links to `docs/development/EVENT_TIMEOUT_RFC.md` which **does not exist**. Will produce a 404.

### WARNINGS

#### D4 - NEXT_STEPS.md still says "release prep in progress"
Header: `"v11.0.0 release prep in progress"` — needs to say RELEASED. Also contains a stale "Tomorrow morning" work log from 2026-04-08 with FAILING CI notes.

#### D5 - DEPRECATION-AUDIT.md shows issues #176/#177/#178 as open
All three shipped in v11.0.0. Should be marked CLOSED/SHIPPED.

#### D6 - PHP 8.1 EOL date wrong in README and CHANGELOG
README.md lines 941, 1004 say "November 25, **2025**". CHANGELOG line 656 says same. The actual EOL was **November 25, 2024**. (The MIGRATION guide at line 8 correctly says 2024.)

#### D7 - `implementation_plan.md` has no COMPLETE status banner, 5 stale TBD markers
Blob/Service/EventPoller struct strategy was "TBD" in the plan — all three shipped in v11.0.0 (CHANGELOG Phase G, I). Unlike `docs/plans/v10.4-v11.0-implementation-plan.md`, there is no `> **Status: COMPLETE**` banner.

#### D8 - `docs/CRITICAL_ANALYSIS_2026.md` describes M3 migration as incomplete
Still lists "resource-to-object migration incomplete" as a Medium finding. V11.0.0 shipped the migration. Needs a "Status as of v11.0.0" update section.

#### D9 - `VERSIONING_STRATEGY.md` Phase 2 status says "Planned" targeting v11.0
Since v11.0 is released, this should be updated to "IMPLEMENTED".

#### D10 - NEXT_STEPS.md has no v11.x roadmap
After listing all v11.0 items as complete, the document ends. No v11.1 or forward-looking section exists.

### INFO (Docs)

- All 13 spec files correctly marked RELEASED/COMPLETE ✅
- `docs/plans/v10.4-v11.0-implementation-plan.md` correctly marked COMPLETE ✅
- `CHANGELOG.md` format is correct (Keep a Changelog 1.1.0), `[11.0.0] - 2026-04-09` is first entry ✅
- `docs/MIGRATION-v10-to-v11.md` is comprehensive with before/after examples and dual-accept bridge mention ✅
- `CONTRIBUTING.md` is accurate, no version-specific issues ✅
- Em dash (—) used ~100+ times in CHANGELOG, README, etc. — no project rule exists prohibiting this
- `VERSIONING_STRATEGY.md` references `VERSION` file (not `VERSION.txt`) — renamed in v10.6.2
- `EXAMPLES.md` line 345 says "NEW in v7.0" — historical, no update critical

---

## Dimension 5 - CI/CD & Release Workflows

**Agent 5 findings from** `.github/workflows/*.yml`, `scripts/`, `docker/`, `config.m4`, `VERSION.txt`.

### PASS checks (no issues)

| Check | Result |
|-------|--------|
| All 12 `uses:` action references | ✅ 100% pinned to SHA256 |
| `VERSION.txt` content | ✅ `11.0.0` |
| `split-stubs.yml` targets | ✅ `satware-main` → `satwareAG/php-firebird-stubs` |
| `coverage.yml` 54% threshold gate | ✅ Enforced with awk float compare |
| `code-quality.yml` runs stubs-sync + version-stamps | ✅ Both scripts present |
| `release-linux.yml` musl builds | ✅ `musllinux_1_2_x86_64` with Firebird from source |
| `release-windows.yml` PHP 8.2-8.5 NTS+TS matrix | ✅ |
| `sanitizers.yml` ASan+UBSan+LSan+TSan | ✅ (ASan runtime limitation documented) |
| `config.m4` version injection | ✅ Reads `VERSION.txt` → `git describe` → `"0.0.0-unknown"` |

### WARNINGS

#### I1 - `codeql.yml` executes mutable third-party shell script (MEDIUM-HIGH)
```bash
FB_SCRIPT_URL="https://raw.githubusercontent.com/IBSurgeon/firebirdlinuxinstall/refs/heads/main/fb_vanilla-50.sh"
curl -fsSL "${FB_SCRIPT_URL}" -o /tmp/fb_install.sh && /tmp/fb_install.sh || true
```
Downloads from `refs/heads/main` (mutable, can change any time). `|| true` suppresses ALL errors. Supply-chain risk in a privileged CI context.
**Fix**: Pin to a specific commit SHA, or mirror the script into `.github/scripts/`.

#### I2 - Curl-pipe Composer install in 3 workflows (MEDIUM)
```bash
curl -sS https://getcomposer.org/installer | php -- --install-dir=/usr/local/bin --filename=composer
```
Present in `ci.yml`, `coverage.yml`, `sanitizers.yml`. Classic "curl | pipe to interpreter" anti-pattern without integrity verification.
**Fix**: Use `shivammathur/setup-php` (already used in `code-quality.yml`) or verify the installer SHA384 hash.

#### I3 - `release-windows.yml` overly broad `contents: write` permission (MEDIUM)
`contents: write` is granted at the workflow level, meaning the `build` job has write access it doesn't need.
**Fix**: Set `permissions: read-all` at workflow level, elevate only in the `release` job.

#### I4 - VERSION / VERSION.txt filename inconsistency (MEDIUM)
`config.m4` reads `VERSION.txt` but `release-linux.yml` and `release-macos.yml` write to `VERSION` (without `.txt`). If config.m4 runs in a release context that hasn't written `VERSION.txt`, it falls back to git describe or "0.0.0-unknown".
**Fix**: Standardize all workflows to write `VERSION.txt`.

#### I5 - Fallback version hardcoded as `"7.0.0"` in all 4 release workflows (MEDIUM)
```bash
VERSION="7.0.0"  # fallback if VERSION file is missing
```
This is 4 major versions stale. A misconfigured run would silently tag artifacts as v7.0.0.
**Fix**: Change to `"11.0.0"` or better, `exit 1` if VERSION.txt is absent.

#### I6 - `clang-tidy` step has `continue-on-error: true` (MEDIUM)
Static analysis findings are silently swallowed - bugprone, cert, and security checks never block merges.
**Fix**: Remove `continue-on-error: true` or route output to PR warning annotations.

#### I7 - `docker/Dockerfile` unpinned base image + PHP 8.5.0RC2 (LOW-MEDIUM)
```dockerfile
FROM debian:bookworm      # floating tag
ARG php_vers="8.1.33 8.2.29 8.3.26 8.4.13 8.5.0RC2"
```
- `debian:bookworm` can silently change between builds — pin to `debian:bookworm@sha256:<digest>`
- `8.5.0RC2` is a release candidate, should not be in a production Dockerfile
- `8.1.33` is listed but PHP 8.1 support was dropped in v7.2.0 — creates confusion

#### I8 - `qa.sh` references non-existent `php81-fb3-dev` service (LOW)
Matrix mode references a Docker Compose service that is not defined in `docker/docker-compose.yml`. PHP 8.1 was dropped. Running `qa.sh matrix` will fail.

### INFO (CI/CD)

- Firebird SDK version `5.0.3`/`5.0.4` must be manually bumped in release workflows on each FB patch — consider a `FIREBIRD_RELEASE.txt` to centralize
- `docker/docker-compose.yml` uses floating `firebird:3`, `firebird:4`, `firebird:5` tags — acceptable for dev, brittle for CI reproducibility
- Coverage threshold at 54% is a minimum floor, not a target — should be raised incrementally
- `check-version-stamps.sh` warns (but does not fail) if a stub file is missing — consider failing hard

---

## Consolidated Fix Priority

### P0 — Fix Immediately (Release Quality)

| ID | File | Action |
|----|------|--------|
| D1 | `README.md:6` | Update version badge `10.6.2` → `11.0.0` |
| D2 | `README.md:933,962,966` | Update "Current Version" section and wget URL |
| D3 | `README.md:929` | Remove or fix broken link to `EVENT_TIMEOUT_RFC.md` |
| C1 | `firebird.c:340` | Fix `arginfo_fbird_delete_user` to require 2 not 3 args |

### P1 — Fix Soon (Correctness/Security)

| ID | File | Action |
|----|------|--------|
| I1 | `.github/workflows/codeql.yml` | Pin IBSurgeon install script to a specific commit SHA |
| I2 | `ci.yml`, `coverage.yml`, `sanitizers.yml` | Replace curl-pipe Composer install |
| I3 | `release-windows.yml` | Scope `contents: write` to release job only |
| I4 | `release-linux.yml`, `release-macos.yml` | Standardize to `VERSION.txt` filename |
| I5 | All release workflows | Change fallback from `"7.0.0"` to `exit 1` |
| S1 | `stubs/firebird-stubs.php`, `phpstan/fbird.stub.php`, `fbird-bootstrap.php` | Fix `FBIRD_VER` to `100` |
| C2 | `fbird_service.c:521` | Move verbose flag bounds check before write |
| D4 | `NEXT_STEPS.md` | Mark v11.0.0 as published, remove stale work log |
| D5 | `docs/DEPRECATION-AUDIT.md` | Mark issues #176/#177/#178 as CLOSED/SHIPPED |

### P2 — Fix in v11.0.x Patch (Quality)

| ID | File | Action |
|----|------|--------|
| C3 | `fbird_batch.c:180` | Complete M3 migration: call `fbird_setup_batch_object()` |
| C4 | `fbird_classes_internal.h` | Remove orphaned/stale header |
| C5 | `fbird_service.c` + `fbird_classes.c` | Unify `fbird_service`/`fbird_service_rsrc` structs |
| C6 | `fbird_connection.c`, `fbird_transaction.c`, `fbird_service.c` | Add error reporting before `RETURN_FALSE` |
| S2 | `stubs/firebird-stubs.php`, `phpstan/fbird.stub.php` | Add `FBIRD_EXCEPTION_MODE_COMPAT` constant |
| S3 | `phpstan/fbird-bootstrap.php` | Add all missing service constants |
| I6 | `code-quality.yml` | Remove `continue-on-error: true` from clang-tidy |
| I7 | `docker/Dockerfile` | Pin base image, remove PHP 8.1 and RC builds |
| D6 | `README.md:941,1004`, `CHANGELOG.md:656` | Fix PHP 8.1 EOL date: 2025 → 2024 |
| D7 | `implementation_plan.md` | Add COMPLETE status banner, resolve TBD markers |
| T5 | `fbird_service_db_mgr.phpt` | Implement RPR_MEND_DB teardown after RPR_VALIDATE_DB |

### P3 — Improve (v11.1.0 targets)

| ID | Area | Action |
|----|------|--------|
| T2 | Test Suite | Expand OOP API tests from 13% to >50% coverage |
| T3 | Test Suite | Update 12 test files from `is_resource()` to dual-bridge pattern |
| T1 | Test Suite | Add `--CLEAN--` to high-risk tests (at minimum: `datatype_001`, `migration_001`, `long_names_*`) |
| T4 | Test Suite | Add live Firebird event test (`fbird_event_wait` with trigger) |
| T6 | Test Suite | Add OOP helper functions to `common.inc` |
| C7 | C Code | Replace `zend_bool` with `bool` throughout (13 usages) |
| C8/C9 | C Code | Add return type annotations to arginfos |
| D8 | Docs | Add "Status as of v11.0.0" section to `CRITICAL_ANALYSIS_2026.md` |
| D9 | Docs | Update `VERSIONING_STRATEGY.md` Phase 2 to "IMPLEMENTED" |
| D10 | Docs | Add v11.x roadmap section to `NEXT_STEPS.md` |
| I8 | CI/CD | Remove `php81-fb3-dev` from `qa.sh` matrix |
| I4 | CI/CD | Pin Docker Compose Firebird images to specific versions |

---

## What's Working Well

- **Actions pinned 100%** - All 12 GitHub Actions `uses:` references use SHA256 hashes ✅
- **Stubs sync perfect** - 89/89 functions, zero phantom stubs, `check-stubs-sync.sh` passes ✅
- **CI quality gates complete** - stubs-sync, version-stamps, PHPStan L8, PHPCS, clang-tidy, cppcheck, Gitleaks all wired ✅
- **No memory safety violations** - no `sprintf`/`strcpy`/`gets`, no `malloc` in request context, no TODO/FIXME markers ✅
- **All 6 OOP classes registered** - Connection, Transaction, ResultSet, Blob, Event, Service all in `fbird_register_classes()` ✅
- **M3 migration 99% complete** - only `fbird_batch_create` still returns raw resource ✅
- **OOP stubs complete** - 10 classes in `firebird-classes.php` (6 core + 3 exceptions + Statement) ✅
- **Sanitizers wired** - ASan, UBSan, LSan, TSan all running; known PHP RTLD_DEEPBIND limitation documented ✅
- **All 13 specs properly marked** RELEASED/COMPLETE ✅
- **SKIPIF coverage 99.2%** - nearly every test properly guarded ✅

---

*Report generated 2026-04-09 by php-firebird QA Swarm (5 parallel agents, 425 tool calls)*
*Agents: C Code Quality | Stubs & PHPStan | Test Suite | Documentation | CI/CD & Release*
