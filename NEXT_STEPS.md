# Next Session Tasks — php-firebird

**Last updated:** 2026-03-22 (EOD session)
**Current version:** 8.0.0
**Branch:** `satware-main`

---

## Status: v8.0.0 Released 🚀

| Item | Status |
|------|--------|
| Phase A — OO API migration (isc_* elimination) | ✅ Complete (PR #111) |
| Phase B — Layer 2 `Firebird\*` OOP classes | ✅ Complete (PR #112) |
| Phase C Part 1 — Legacy debt cleanup (`fb_safe_handle` removal) | ✅ Complete (PR #113) |
| Phase C Part 2 — Layer 3 `pdo_fbird` PDO driver | ✅ Complete (PR #114) |
| Phase D — Stubs, docs, VERSION 8.0.0 | ✅ Complete (PR #115) |
| Issue #107 — Extension version missing | ✅ Closed (fixed in v8.0.0) |
| Issue #108 — fbird_pconnect_001 test failure | ✅ Closed (fixed in v8.0.0) |
| v8.0.0 release artifacts (Linux + Windows) | ✅ Published — all 16 artifacts embed `8.0.0` |

**CI matrix: 12 combinations green** — PHP 8.2/8.3/8.4/8.5 × FB 3.0/4.0/5.0

---

## Architecture: 3-Layer Design

```
Layer 1: fbird_*()          — Procedural API (full Firebird feature set)
Layer 2: Firebird\*         — OOP classes (Connection, Transaction, Statement,
                               ResultSet, Blob, Service, Exception hierarchy)
Layer 3: pdo_fbird.so       — PDO driver with fbird: DSN prefix
```

All layers use the Firebird 3.0+ OO API internally — zero legacy `isc_*` calls.

---

## EOD Session — 2026-03-22

| Item | Status |
|------|--------|
| `.gitignore`: ignore `.env` and `docker/.env` | ✅ Done (commit `7b7f7a0`) |
| EOD protocol: `Workflows/eod.hygiene-git.standards.md` | ✅ Done |
| EOD protocol: `Workflows/eod.knowledge-documentation.md` | ✅ Done |
| EOD protocol: `Workflows/eod.ops-automation.md` | ✅ Done |
| EOD automation: `scripts/daily-routine.sh eod` | ✅ Done |

Run `bash scripts/daily-routine.sh eod` to execute the full EOD checklist.

---

## Open Items

### doctrine-firebird-driver

- **Issue #95** — Migration roadmap filed: upgrade from php-firebird v7.3.0 → v8.0.0 (filed 2026-03-22)
- **Issue #50** — Schema test transaction deadlocks (bug — fix first)
- **Issue #47** — Simplify test suite (drop PHP 8.1 compat code)
- **Issue #20** — Release v3.11.0 (Configurable LIKE CAST Length)

### php-firebird future work

- **pdo_fbird**: Add named cursor support, scrollable result sets
- **Firebird\Events**: Full async event API in Layer 2
- **Firebird\Array**: Array field support in Layer 2
- **Statement migration**: Migrate `fbird_query_prepare.c` / `fbird_query_exec.c` to `IStatement` OO API (removes last `isc_dsql_*` calls)
- **Split-stubs workflow**: Fix tag collision in target repo (pre-existing CI issue)

---

## Validation Baseline (v8.0.0)

| Check | Result |
|-------|--------|
| Test matrix (12 targets) | ✅ 12/12 PASS |
| AddressSanitizer | ✅ 3/3 PASS |
| Valgrind | ✅ 0 definitely/indirectly lost bytes |
| Release artifacts | ✅ 16/16 embed `8.0.0` |
