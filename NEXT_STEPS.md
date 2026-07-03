# Next Steps - php-firebird

**Last Updated**: 2026-07-03 (v11.1.0 shipped; starting v12.0.0 - OOP API Completion)

> **Status**: v11.1.0 shipped (tag `v11.1.0`, published 2026-07-02). All QA hardening items closed
> (QH-1 through QH-8, #233 batch OOP migration, zend_bool elimination, stubs completeness).
> v12.0.0 - OOP API Completion milestone started (milestone #21, due 2026-09-29).

## Immediate (v10.6.x)

- [x] v10.6.1 released - musl bundle collision, zend_get_type_by_const portability, Windows release fix
- [x] `docker/musl-debug/` scratch directory removed (dir did not exist, confirmed 2026-04-02)
- [x] v10.6.2 tagged - current stable release on `v10.6.x` branch
- [x] `doctrine-firebird-driver` issues #105 and #106 created and amended - FB 2.5-5.0 compatibility story, type mapping validation, PDO clarification, CI modernization (satwareAG/doctrine-firebird-driver team to action)
- [x] v10.6.2 published - all release pipelines green, 100+ assets uploaded, #213 closed (2026-04-07)

## v11.0 Modernization (Next Milestone)

Tracked via GitHub issues #175-#179 (milestone: v11.0 - Modernization, due 2026-12-31):

- [x] #175 - [M2] Migrate all 84 procedural arginfo to typed return macros (CLOSED)
- [x] #176 - [M3] Resource-to-object migration for Layer 1 procedural functions (MERGED via PR #215)
  - [x] Phase A+B (`7f100f6`): Class skeletons - Firebird\Connection, \Transaction, \Event, \Batch
  - [x] Phase C (`1b37219`): fbird_connect/pconnect/create_database return Firebird\Connection
  - [x] Phase D (`508f75e`): fbird_trans/trans_start return Firebird\Transaction
  - [x] Phase E (`351d75d`): Dual-accept variadic loops in fbird_trans/query/prepare
  - [x] Phase F: fbird_query/execute/fetch return Firebird\ResultSet + dual-accept bridges (le_query → object)
  - [x] Phase G: fbird_blob_* return Firebird\Blob (le_blob → object)
    - [x] G1 (`3ddee6d`): Blob weak-ref infrastructure, FBIRD_VALIDATE_BLOB_EX macro
    - [x] G2 (`5be2453`): Dual-accept all consuming blob functions
    - [x] G3 (`dc2dde7`, `7761c40`): fbird_blob_create/open/create_seekable/open_seekable return Firebird\Blob; stubs updated
  - [x] Phase H: fbird_event_* return Firebird\Event (le_event → object)
  - [x] Phase I: fbird_service_* return Firebird\Service (le_service → object)
  - [x] Dual-accept sweep: fbird_commit/rollback/close/affected_rows accept objects too
- [x] #177 - [M12] Replace call_user_function() in OOP layer with direct C calls (CLOSED)
- [x] #178 - [L3] Remove dead FB_API_VER < 30 code paths (CLOSED)
- [x] #179 - [L2] Remove legacy gds32_ms fallback from config.w32 (CLOSED)

## Open Implementation Specs

All previously open specs confirmed RELEASED and closed (2026-04-07 audit):

- [x] `spec-v10.3.7-security.md`: All criteria met - shipped in v10.3.7 (commit 6c937a2)
- [x] `spec-v10.4-build-hardening.md`: All criteria met - shipped in v10.4.x (commit 99c7c0f)
- [x] `spec-v10.4-supply-chain.md`: All criteria met - SLSA, SBOM, Dependabot, version stamps shipped

## 2026-04-09 AM - PHPStan Code Quality Fix (COMPLETE)

**PR #215 merged** (squash, ff84b3a). M3 Phases A-I complete. #176 closed.

- PHPStan stub fixes: duplicate class declarations removed, `@return`/`@param` updated for M3
- PHPCS fix: `Database.php` constructor `) {` on same line (PSR-12)
- Stale open PRs #209/#210/#211 rebased and merged (v11 housekeeping)

## 2026-04-08 EOD - M3 Phase I + CI Fix (ARCHIVED — resolved 2026-04-09)

All items resolved: CI green with `83b4de6`, PR #215 merged (squash `ff84b3a`), issue #176 closed,
v11.0.0 tagged and released on 2026-04-09. Full work log archived in git history.

## 2026-04-07 - v10.6.2 Published

Release published live: https://github.com/satwareAG/php-firebird/releases/tag/v10.6.2
Issue #213 closed. All pipelines (Linux/macOS/Windows/Stubs/Coverage/Sanitizers) completed successfully.

---

## Forward Roadmap (v11.x+)

### v11.1.0 - Quality and Security Hardening (SHIPPED 2026-07-02)

All QA audit bugs closed. Milestone #20 fully complete (21 closed / 0 open).

- [x] #233 - Complete M3 migration: `fbird_batch_create` returns `Firebird\BatchHandle` (CLOSED)
  - `fbird_batch.c:192` now calls `fbird_setup_batch_object()`; zero `RETVAL_RES` in batch code

### v12.0.0 - OOP API Completion (in progress, milestone #21, due 2026-09-29)

Major version (non-breaking). OOP API test infra completion, Phase H Event implementation,
typed arginfo, test coverage overhaul. Also includes `fbird_classes.c` split into per-class files.

**Audit findings (2026-07-03):**
- `zend_bool` -> `bool` (OC-9): **DONE** in v11.1.0 - removed from v12 scope
- M6 fuzz dictionary: **DONE** - `fuzz/dictionary/sql.dict` exists (372 lines) - removed from v12 scope
- `fbird_classes_internal.h` deletion (QH-4b): **DONE** - removed from v12 scope

**Implementation issues (13 total):**

- [ ] #250 - Implement `Firebird\Event` class methods (Phase H) - **blocker for #247**
- [ ] #247 - Add live Firebird event test using fbird_event_wait with actual DB trigger
- [ ] #244 - Expand OOP API test coverage from 13% to >50%
- [ ] #245 - Add `--CLEAN--` sections to all state-modifying tests (154 more needed, target >80% of 284)
- [ ] #246 - Update 10 test files from `is_resource()` to `instanceof Firebird\*` dual-bridge
- [ ] #249 - OOP-aware helper functions in `tests/common.inc`
- [ ] #252 - Typed arginfo: 159 procedural parameters + 9 OOP method return types
- [ ] #248 - Implement RPR_MEND_DB teardown after RPR_VALIDATE_DB in fbird_service_db_mgr.phpt
- [ ] #257 - Windows builds missing DLLs for some PHP versions
- [ ] #258 - `php_pdo_unregister_driver` undefined symbol on Rocky Linux (#150 regression)
- [ ] L1 - Enable `-flto=auto` in `config.m4` for all builds (Linux workflow-only currently)
- [ ] M8 - Replace pointer smoking in `IBG(status[])` with proper output parameter (172 usages)
- [ ] OC-11 - Unify `fbird_service` and `fbird_service_obj` structs (carry-over from QH-4c)

**Structural refactor (prerequisite for Phase H):**
- [ ] Split `fbird_classes.c` (1450 lines, 8 classes) into `fbird_class_{connection,transaction,statement,resultset,blob,batch,service,event}.c` + registry stub

### Backlog (unscheduled)

- [ ] macOS x86_64 builds disabled (macos-13 runner deprecated) - evaluate macos-15 x86_64 when available
- [ ] Coverage threshold still at 54% - target 65%+ with v12.0 test improvements
