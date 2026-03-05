# Next Session Tasks — php-firebird

**Last updated:** 2026-03-05
**Current version:** 7.2.0 (released 2026-03-04)
**Branch:** `satware-main`

---

## Status: v7.2.0 Released ✅ | PR #96 in flight

| Item | Status |
|------|--------|
| v7.2.0 release | ✅ Published |
| Stubs v7.2.0 (Packagist) | ✅ Tagged |
| PR #96: CI matrix 4→7 combinations (FB4 + PHP 8.3) | 🔄 CI passing, ready to merge |

---

## Priority 1 — Merge PR #96

```bash
# Verify all 7 CI jobs pass first
gh pr checks 96 --repo satwareAG/php-firebird | grep -E "^PHP"

# Merge
gh pr merge 96 --repo satwareAG/php-firebird --squash --delete-branch
```

---

## Priority 2 — php-firebird v7.3.0 Milestone

**Milestone**: https://github.com/satwareAG/php-firebird/milestone/3

| Issue | Title | Priority |
|-------|-------|----------|
| #97 | feat(ci): PHP 8.3 full CI row (FB 3.0 + FB 5.0) | High — complete the matrix |
| #98 | feat(ci): PHP 8.5 day-1 support | Medium — Dockerfile-8.5 exists |
| #99 | chore(ci): update pinned FB client versions to latest patch | Low — maintenance |

**Start with #97** — extends PR #96 work, adds PHP 8.3 × FB 3.0 and PHP 8.3 × FB 5.0 entries.

---

## Priority 3 — doctrine-firebird-driver v3.11.0

**Milestone**: https://github.com/satwareAG/doctrine-firebird-driver/milestone/7

| Issue | Title | Priority |
|-------|-------|----------|
| #20 | Release v3.11.0 - Configurable LIKE CAST Length | High — release blocker |
| #47 | Simplify test suite (php-firebird v7 minimum) | High — now actionable |
| #44 | Is BLOB streaming workaround still needed in v7? | Medium — investigate |
| #50 | Schema test transaction deadlocks | Bug — fix first |
| #51 | PHPUnit test timeouts for schema operations | Enhancement |
| #53 | Verify phpunit.sh TTY exit code fix | Testing |
| #42 | Use fbird_escape_string() for SQL string escaping | Low |
| #43 | Add INT128 and DECFLOAT type mappings (FB 4.0+) | Low |
| #46 | Add functional tests for IBatch API (FB 4.0+) | Low |
| #48 | Document SQLSTATE error handling improvements | Docs |

**Start with #47** — simplifying the test suite (dropping PHP 8.1 compat code) unblocks many other issues.

---

## Priority 4 — doctrine-firebird-driver v4.0.0-planning

**Milestone**: https://github.com/satwareAG/doctrine-firebird-driver/milestone/8

| Issue | Title | Notes |
|-------|-------|-------|
| #78 | Sprint 5 [TRACKING] DBAL 4.x forward-compatibility | Tracking issue — convert to milestone when DBAL 4.x stable |
| #38 | Windows CI Testing | Blocked by php-firebird Windows DLLs |

---

## Milestone State (post-2026-03-05 hygiene)

### php-firebird

| Milestone | State | Issues |
|-----------|-------|--------|
| v7.0.0 | ✅ Closed | 26 closed |
| v7.2.0 | ✅ Closed | 6 closed |
| v7.3.0 | 🔄 Open | 3 open (#97, #98, #99) |

### doctrine-firebird-driver

| Milestone | State | Issues |
|-----------|-------|--------|
| Sprint 1-5 | ✅ All Closed | All issues resolved |
| v3.10.0 | ✅ Closed | 1 closed |
| v3.11.0 | 🔄 Open | 10 open |
| v4.0.0-planning | 🔄 Open | 2 open (#78, #38) |

---

## Context

- **php-firebird release:** https://github.com/satwareAG/php-firebird/releases/tag/v7.2.0
- **Stubs release:** https://github.com/satwareAG/php-firebird-stubs/releases/tag/v7.2.0
- **PR #96:** https://github.com/satwareAG/php-firebird/pull/96
- **v7.3.0 milestone:** https://github.com/satwareAG/php-firebird/milestone/3
- **doctrine v3.11.0 milestone:** https://github.com/satwareAG/doctrine-firebird-driver/milestone/7
- **doctrine v4.0.0-planning:** https://github.com/satwareAG/doctrine-firebird-driver/milestone/8
