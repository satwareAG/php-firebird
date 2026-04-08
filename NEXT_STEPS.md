# Next Steps - php-firebird

**Last Updated**: 2026-04-08 EOD (M3 Phase I complete + CI fix pushed)

## Immediate (v10.6.x)

- [x] v10.6.1 released - musl bundle collision, zend_get_type_by_const portability, Windows release fix
- [x] `docker/musl-debug/` scratch directory removed (dir did not exist, confirmed 2026-04-02)
- [x] v10.6.2 tagged - current stable release on `v10.6.x` branch
- [x] `doctrine-firebird-driver` issues #105 and #106 created and amended - FB 2.5-5.0 compatibility story, type mapping validation, PDO clarification, CI modernization (satwareAG/doctrine-firebird-driver team to action)
- [x] v10.6.2 published - all release pipelines green, 100+ assets uploaded, #213 closed (2026-04-07)

## v11.0 Modernization (Next Milestone)

Tracked via GitHub issues #175-#179 (milestone: v11.0 - Modernization, due 2026-12-31):

- [x] #175 - [M2] Migrate all 84 procedural arginfo to typed return macros (CLOSED)
- [ ] #176 - [M3] Resource-to-object migration for Layer 1 procedural functions (breaking change)
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

## Technical Debt

- [ ] macOS x86_64 builds disabled (macos-13 runner deprecated) - evaluate macos-15 x86_64 when available
- [ ] Coverage threshold still at 54% - target 65%+ with v11.0 test improvements

## 2026-04-08 EOD - M3 Phase I + CI Fix

**Branch**: `feat/20260408_morning_start` - ready for PR review.

### Today's commits (HEAD → satware-main delta)

- `83b4de6` fix(ci): remove dead fbird_batch stubs from #else block - FB3.0 build fix
- `69edc28` docs: update README version badge and compatibility section to 10.6.2
- `0d0de46` docs: update CHANGELOG for M3 Phase H + Phase I service migration
- `8ec1e2a` fix(m3): fix remaining test failures for resource-to-object migration (#176)

### CI status at EOD

- Memory Sanitizers: PASS
- CI: FAILING (triggered re-run with `83b4de6` fix) - check tomorrow AM
- Linux Code Coverage: FAILING (likely same root cause - check after CI fix lands)
- Root cause: `fbird_classes.c` `#else` stubs used `fbird_batch *` (undefined in FB3.0 headers)

### Tomorrow morning

1. Check CI status for `feat/20260408_morning_start` - expect green after `83b4de6`
2. If CI green: open PR to merge into `satware-main`
3. Next M3 phase: close issue #176 once PR merged
4. Remaining active branches: `feat/175-typed-arginfo`, `feat/v11-dead-code-removal`, `wip/spec-implementation-security-hardening`

## 2026-04-07 - v10.6.2 Published

Release published live: https://github.com/satwareAG/php-firebird/releases/tag/v10.6.2
Issue #213 closed. All pipelines (Linux/macOS/Windows/Stubs/Coverage/Sanitizers) completed successfully.
