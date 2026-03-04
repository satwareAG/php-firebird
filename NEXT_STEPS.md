# Next Session Tasks — php-firebird

**Last updated:** 2026-03-04 (PR #95 open — issue #90 awaiting CI)
**Current version:** 7.1.0 (released)
**Branch:** `satware-main`

---

## Priority 0 — ✅ RESOLVED

- [x] **`fix/transaction-mshutdown-uaf-78-79`** — fix confirmed merged into `satware-main`
  - Squash-merged as commit `5bcbc9f` / PR #81 (2026-03-03)
  - Issues #78 and #79 are closed

---

## Priority 1 — ✅ RESOLVED

- [x] Release (Linux) re-run completed successfully (2026-03-04)
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

## Priority 3 — ✅ v7.2.0 breaking changes complete

All three breaking change issues implemented and PRs open:

1. **#92 — remove `ibase_*` aliases** ✅ PR [#93](https://github.com/satwareAG/php-firebird/pull/93) merged (commit `1301bf5`)
2. **#91 — drop Firebird 2.5** ✅ PR [#94](https://github.com/satwareAG/php-firebird/pull/94) merged (commit `df10835`)
3. **#90 — drop PHP 8.1** ✅ PR [#95](https://github.com/satwareAG/php-firebird/pull/95) open — awaiting CI
   - Branch: `feat/drop-php81-90` (commit `78bb89e`)
   - Changes: docker-compose.yml, Dockerfile-8.1 deleted, ci.yml, composer.json, stubs/composer.json, constitution.md, README.md, CHANGELOG.md

---

## Next immediate action (next session start)

1. Check CI on PR #95: `gh pr checks 95 --repo satwareAG/php-firebird`
2. If CI green: `gh pr merge 95 --repo satwareAG/php-firebird --squash --delete-branch`
3. Sync local: `git checkout satware-main && git pull origin satware-main`
4. **Release v7.2.0**: All three breaking changes merged → tag and release
   ```bash
   # Update VERSION file
   echo "7.2.0" > VERSION
   git add VERSION && git commit -m "chore(release): bump version to 7.2.0"
   # Update CHANGELOG.md [Unreleased] → [7.2.0] - YYYY-MM-DD
   # Tag and push
   git tag v7.2.0 && git push origin satware-main --tags
   ```

---

## Context

- **Release:** https://github.com/satwareAG/php-firebird/releases/tag/v7.1.0
- **CHANGELOG:** https://github.com/satwareAG/php-firebird/blob/satware-main/CHANGELOG.md
- **Test suite:** 173/173 pass (0 fail, 4 skipped) — as of v7.1.0
- **Stubs:** 86/86 in sync
- **v7.2.0 milestone:** https://github.com/satwareAG/php-firebird/milestone/2
