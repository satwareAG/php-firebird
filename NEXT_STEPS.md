# Next Session Tasks — php-firebird

**Last updated:** 2026-03-04 (v7.2.0 released)
**Current version:** 7.2.0 (released 2026-03-04)
**Branch:** `satware-main`

---

## Status: v7.2.0 Released ✅

All v7.2.0 breaking changes shipped and release published:

| Change | Issue | PR | Status |
|--------|-------|----|--------|
| Remove `ibase_*` aliases | #92 | #93 | ✅ Merged |
| Drop Firebird 2.5 support | #91 | #94 | ✅ Merged |
| Drop PHP 8.1 support | #90 | #95 | ✅ Merged |
| Release tag + GitHub Release | — | — | ✅ Published |
| Stubs v7.2.0 tag (Packagist) | — | — | ✅ Tagged |

---

## Downstream Compatibility PRs/MRs

| Repo | PR/MR | Status | Notes |
|------|-------|--------|-------|
| `satwareAG/doctrine-firebird-driver` | PR #81 | ⚠️ Pre-existing failures | Test failures exist on `3.0.x` base too — not regressions from v7.2.0 compat |
| `satware/satag-amicron-entity-bundle` | MR !25 | ✅ Pipeline green | Ready to merge |
| `satware/amicron-platform` | MR !116 | 🔄 Pipeline running | Fixed: `^3.10` → `^3.0` (v3.10.0 not yet stable-released on Packagist) |

**Root causes fixed:**
1. `satwareag/php-firebird-stubs` was missing the `v7.2.0` tag → tagged manually → Packagist propagated ✅
2. `amicron-platform` used `satag/doctrine-firebird-driver: ^3.10` but only `3.0.2` is stable on Packagist → changed to `^3.0` ✅
3. `doctrine-firebird-driver` PR #81 test failures are pre-existing on `3.0.x` base branch (not regressions)

---

## Priority 1 — Merge downstream PRs/MRs (once CI green)

- [ ] Merge `satwareAG/doctrine-firebird-driver` PR #81
  ```bash
  gh pr merge 81 --repo satwareAG/doctrine-firebird-driver --squash --delete-branch
  ```
- [ ] Merge `satware/satag-amicron-entity-bundle` MR !25 (pipeline already green)
- [ ] Merge `satware/amicron-platform` MR !116 (after pipeline passes)

---

## Priority 2 — Close completed milestones

- [ ] Close `php-firebird` v7.2.0 milestone (6/6 issues closed, 0 open)
  ```bash
  gh api --method PATCH repos/satwareAG/php-firebird/milestones/2 -f state=closed
  ```
- [ ] Close `doctrine-firebird-driver` v3.10.0 milestone (1/1 closed, 0 open)

---

## Priority 3 — doctrine-firebird-driver open issues (post-v7.2.0)

| Issue | Title | Priority |
|-------|-------|----------|
| #78 | Sprint 5 [TRACKING] DBAL 4.x forward-compatibility | High — tracking issue |
| #47 | Simplify test suite now php-firebird v7 is minimum | Medium — now actionable |
| #44 | Is BLOB streaming workaround still needed in v7? | Medium — investigate |
| #20 | Release v3.11.0 - Configurable LIKE CAST Length | Medium |
| #43 | Add INT128 and DECFLOAT type mappings (FB 4.0+) | Low |
| #42 | Use fbird_escape_string() for SQL string escaping | Low |
| #46 | Add functional tests for IBatch API (FB 4.0+) | Low |
| #50 | Schema test transaction deadlocks | Bug |
| #51 | PHPUnit test timeouts for schema operations | Enhancement |
| #53 | Verify phpunit.sh TTY exit code fix | Testing |
| #48 | Document SQLSTATE error handling improvements | Docs |
| #38 | Windows CI Testing: Blocked by php-firebird Windows DLLs | Enhancement |

**Note:** Issue #47 ("Simplify test suite when php-firebird v7 becomes minimum") is now
actionable — php-firebird v7.2.0 is the minimum, PHP 8.1 is dropped.

---

## Context

- **Release:** https://github.com/satwareAG/php-firebird/releases/tag/v7.2.0
- **Stubs release:** https://github.com/satwareAG/php-firebird-stubs/releases/tag/v7.2.0
- **CHANGELOG:** https://github.com/satwareAG/php-firebird/blob/satware-main/CHANGELOG.md
- **v7.2.0 milestone:** https://github.com/satwareAG/php-firebird/milestone/2
- **Breaking changes:** PHP ≥8.2, Firebird ≥3.0, `fbird_*` only (no `ibase_*`)
