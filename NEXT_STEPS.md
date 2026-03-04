# Next Session Tasks — php-firebird

**Last updated:** 2026-03-04 (updated 2026-03-04 — v7.2.0 milestone + issues created)  
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

## Priority 1 — ✅ RESOLVED

- [x] Release (Linux) re-run `22668523927` completed successfully (2026-03-04)
  - All 10 build jobs + release job: ✓
  - Root cause was transient HTTP 504 on Firebird SDK download (GitHub CDN)
  - v7.1.0 release artifacts: 29/29 complete

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

## Priority 3 — ✅ v7.2.0 planning complete

- [x] Milestone `v7.2.0` created on GitHub (milestone #2)
- [x] Closed stale `v7.0.0` milestone (0 open issues)
- [x] Created label `breaking change` (#B60205)
- [x] Issue [#90](https://github.com/satwareAG/php-firebird/issues/90) — `breaking: drop PHP 8.1 support`
- [x] Issue [#91](https://github.com/satwareAG/php-firebird/issues/91) — `breaking: drop Firebird 2.5 server support`
- [x] Issue [#92](https://github.com/satwareAG/php-firebird/issues/92) — `breaking: remove ibase_* function aliases`

### v7.2.0 Implementation Order (recommended)

1. **#92 — remove `ibase_*` aliases** ✅ PR [#93](https://github.com/satwareAG/php-firebird/pull/93) open — awaiting CI
2. **#91 — drop Firebird 2.5** (remove compat shims + CI matrix reduction)
3. **#90 — drop PHP 8.1** (CI matrix reduction, update constraints)

Each issue should be a separate feature branch + PR targeting `satware-main`.

---

## Context

- **Release:** https://github.com/satwareAG/php-firebird/releases/tag/v7.1.0
- **CHANGELOG:** https://github.com/satwareAG/php-firebird/blob/satware-main/CHANGELOG.md
- **Test suite:** 173/173 pass (0 fail, 4 skipped)
- **Stubs:** 86/86 in sync
- **v7.2.0 milestone:** https://github.com/satwareAG/php-firebird/milestone/2
