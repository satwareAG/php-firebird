# Next Session Tasks — php-firebird

**Last updated:** 2026-03-04 (updated 2026-03-04 — Priority 0 resolved)  
**Current version:** 7.1.0 (released)  
**Branch:** `satware-main`

---

## Priority 0 — ✅ RESOLVED

- [x] **`fix/transaction-mshutdown-uaf-78-79`** — fix confirmed merged into `satware-main`
  - Squash-merged as commit `5bcbc9f` / PR #81 (2026-03-03)
  - Author: Michael Wegener, Co-authored-by: Jane Alesi
  - Patch in `b68d85f` (local branch) was byte-for-byte identical to `5bcbc9f` on `satware-main`
  - Local branch deleted (remote was already gone); fix shipped in v7.1.0
  - Issues #78 and #79 are closed

---

## Priority 1 — Monitor (no action needed unless failure)

- [ ] Verify re-run of Release (Linux) `22668523927` completes successfully
  - Root cause: transient HTTP 504 on Firebird SDK download from GitHub releases
  - All 29 release artifacts already uploaded by prior successful runs
  - Re-run triggered: `gh run rerun 22668523927 --repo satwareAG/php-firebird --failed`

---

## Priority 2 — Downstream upgrades (track progress)

- [ ] `satwareAG/doctrine-firebird-driver` — issue [#80](https://github.com/satwareAG/doctrine-firebird-driver/issues/80)
  - Bump `ext-firebird` to `^7.1.0`
  - Integrate `fbird_query_params_tx` for DBAL 4.x
  - Remove PHP 8.1 from CI matrix (deprecated)
- [ ] `satware/satag-amicron-entity-bundle` — issue [#49](https://gitlab.satware.com/satware/satag-amicron-entity-bundle/-/issues/49)
  - Verify `ext-firebird ^7.1.0` on deployment servers
  - Migrate any `ibase_*` calls to `fbird_*`
- [ ] `satware/amicron-platform` — issue [#67](https://gitlab.satware.com/satware/amicron-platform/-/issues/67)
  - Same as entity-bundle + coordinate upgrade

---

## Priority 3 — v7.2.0 planning

- [ ] Remove PHP 8.1 support (EOL Nov 2025, deprecated in v7.1.0)
- [ ] Remove Firebird 2.5 server connectivity (deprecated in v7.1.0)
- [ ] Remove `ibase_*` function aliases (deprecated in v7.1.0, no longer tested)
- [ ] Open milestone `v7.2.0` on GitHub with breaking change issues

---

## Context

- **Release:** https://github.com/satwareAG/php-firebird/releases/tag/v7.1.0
- **CHANGELOG:** https://github.com/satwareAG/php-firebird/blob/satware-main/CHANGELOG.md
- **Test suite:** 173/173 pass (0 fail, 4 skipped)
- **Stubs:** 86/86 in sync
