# Next Session Tasks — php-firebird

**Last updated:** 2026-03-20
**Current version:** 7.3.5-dev (post-7.3.4 release)
**Branch:** `satware-main`

---

## Status: v7.3.4 Released 🚀

| Item | Status |
|------|--------|
| v7.3.2 release | ✅ Published (Deduplication, Service API fixes) |
| v7.3.3 release | ✅ Published (Version stabilization) |
| v7.3.4 release | ✅ Published (Final stabilization) |
| fbird_blob_info() fix | ✅ Merged (Fixed SIGSEGV with resource handles) |
| issue23 stability | ✅ Verified (10/10 passes on PHP 8.5 / FB 4.0) |

**CI matrix is now 12 combinations:** PHP 8.2/8.3/8.4/8.5 × FB 3.0/4.0/5.0

---

## Known Flaky Test (Stabilized)

`tests/issue23_alias_padding_001.phpt` (PHP 8.5 / FB 4.0)
Verified stability in current session (2026-03-20) with 10 consecutive passes in a fresh PHP 8.5 environment.
The metadata deduplication fix in v7.3.2 (using `zend_symtable_str_update`) appears to have fully resolved the non-determinism.

**Action:** Continue monitoring in CI, but primary stabilization is complete.

---

## Priority 1 — doctrine-firebird-driver v3.11.0

**Milestone**: https://github.com/satwareAG/doctrine-firebird-driver/milestone/7

Start order:
1. **Issue #50** — Schema test transaction deadlocks (bug — fix first)
2. **Issue #47** — Simplify test suite (drop PHP 8.1 compat code) — now actionable since php-firebird v7.2.0 dropped PHP 8.1
3. **Issue #20** — Release v3.11.0 - Configurable LIKE CAST Length (release blocker)

| Issue | Title | Priority |
|-------|-------|----------|
| #50 | Schema test transaction deadlocks | Bug — fix first |
| #47 | Simplify test suite (php-firebird v7 minimum) | High — now actionable |
| #20 | Release v3.11.0 - Configurable LIKE CAST Length | High — release blocker |
| #44 | Is BLOB streaming workaround still needed in v7? | Medium — investigate |
| #51 | PHPUnit test timeouts for schema operations | Enhancement |
| #53 | Verify phpunit.sh TTY exit code fix | Testing |
| #42 | Use fbird_escape_string() for SQL string escaping | Low |
| #43 | Add INT128 and DECFLOAT type mappings (FB 4.0+) | Low |
| #46 | Add functional tests for IBatch API (FB 4.0+) | Low |
| #48 | Document SQLSTATE error handling improvements | Docs |

---

## Priority 2 — doctrine-firebird-driver v4.0.0-planning

**Milestone**: https://github.com/satwareAG/doctrine-firebird-driver/milestone/8

| Issue | Title | Notes |
|-------|-------|-------|
| #78 | Sprint 5 [TRACKING] DBAL 4.x forward-compatibility | Tracking issue — convert to milestone when DBAL 4.x stable |
| #38 | Windows CI Testing | Blocked by php-firebird Windows DLLs |

---

## Milestone State (post-2026-03-06)

### php-firebird

| Milestone | State | Issues |
|-----------|-------|--------|
| v7.0.0 | ✅ Closed | 26 closed |
| v7.2.0 | ✅ Closed | 6 closed |
| v7.3.0 | ✅ Closed | 4 closed (#97, #98, #99, #104) |

### doctrine-firebird-driver

| Milestone | State | Issues |
|-----------|-------|--------|
| Sprint 1-5 | ✅ All Closed | All issues resolved |
| v3.10.0 | ✅ Closed | 1 closed |
| v3.11.0 | 🔄 Open | 10 open |
| v4.0.0-planning | 🔄 Open | 2 open (#78, #38) |

---

## Context

- **php-firebird repo:** https://github.com/satwareAG/php-firebird
- **php-firebird release:** https://github.com/satwareAG/php-firebird/releases/tag/v7.3.0
- **Stubs release:** https://github.com/satwareAG/php-firebird-stubs/releases/tag/v7.3.0
- **v7.3.0 milestone:** https://github.com/satwareAG/php-firebird/milestone/3 (closed)
- **doctrine v3.11.0 milestone:** https://github.com/satwareAG/doctrine-firebird-driver/milestone/7
- **doctrine v4.0.0-planning:** https://github.com/satwareAG/doctrine-firebird-driver/milestone/8
