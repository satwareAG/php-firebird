# Next Session Tasks — php-firebird

**Last updated:** 2026-03-04 (PR #94 open — issue #91 awaiting CI)
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

## Priority 3 — v7.2.0 implementation in progress

- [x] Milestone `v7.2.0` created on GitHub (milestone #2)
- [x] Closed stale `v7.0.0` milestone (0 open issues)
- [x] Created label `breaking change` (#B60205)
- [x] Issue [#90](https://github.com/satwareAG/php-firebird/issues/90) — `breaking: drop PHP 8.1 support`
- [x] Issue [#91](https://github.com/satwareAG/php-firebird/issues/91) — `breaking: drop Firebird 2.5 server support`
- [x] Issue [#92](https://github.com/satwareAG/php-firebird/issues/92) — `breaking: remove ibase_* function aliases`

### v7.2.0 Implementation Order

1. **#92 — remove `ibase_*` aliases** ✅ PR [#93](https://github.com/satwareAG/php-firebird/pull/93) merged (2026-03-04, commit `1301bf5`)
2. **#91 — drop Firebird 2.5** ✅ PR [#94](https://github.com/satwareAG/php-firebird/pull/94) open — awaiting CI
   - Branch: `feat/drop-firebird-25-91` (commit `1ab7585`)
   - Changes: docker-compose.yml, ci.yml, README.md, CHANGELOG.md
   - No C source changes (all `FB_API_VER < 30` are already `#error` guards)
3. **#90 — drop PHP 8.1** (next after PR #94 merges)

---

## Next immediate action (next session start)

1. Check CI on PR #94: `gh pr checks 94 --repo satwareAG/php-firebird`
2. If CI green: `gh pr merge 94 --repo satwareAG/php-firebird --squash --delete-branch`
3. Sync local: `git checkout satware-main && git pull origin satware-main`
4. Begin issue #90 (drop PHP 8.1):
   - `git checkout -b feat/drop-php81-90 satware-main`

### Issue #90 checklist (drop PHP 8.1)

- [ ] Remove PHP 8.1 from Docker build matrix (`docker/php/Dockerfile-8.1`, `docker/docker-compose.yml`)
- [ ] Remove PHP 8.1 from CI test matrix (`.github/workflows/ci.yml`, `coverage.yml`, `sanitizers.yml`)
- [ ] Update `composer.json` PHP constraint to `>=8.2`
- [ ] Update `stubs/composer.json` PHP constraint to `>=8.2`
- [ ] Update `README.md` supported PHP versions (remove 8.1 deprecation notice, update minimum)
- [ ] Update `constitution.md` Article V (PHP 8.1 listed as supported)
- [ ] Add entry to `CHANGELOG.md` under `[Unreleased]`
- [ ] Run `bash scripts/check-stubs-sync.sh` — must exit 0

---

## Context

- **Release:** https://github.com/satwareAG/php-firebird/releases/tag/v7.1.0
- **CHANGELOG:** https://github.com/satwareAG/php-firebird/blob/satware-main/CHANGELOG.md
- **Test suite:** 173/173 pass (0 fail, 4 skipped) — as of v7.1.0
- **Stubs:** 86/86 in sync
- **v7.2.0 milestone:** https://github.com/satwareAG/php-firebird/milestone/2
