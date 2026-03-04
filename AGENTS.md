# AGENTS.md — php-firebird AI Agent Context

> This file provides context for AI coding agents (Cline, GitHub Copilot, etc.)
> working on the php-firebird project.
> **Read `.specify/memory/constitution.md` first** — it governs all decisions.

---

## Project Identity

- **Name**: php-firebird — Native PHP extension for Firebird databases
- **Language**: C (Zend Engine API) + C++ (internal only)
- **Version**: 7.1.0 (current stable)
- **Branch**: `satware-main` (stable), feature branches for all work
- **Repo**: https://github.com/satwareAG/php-firebird

---

## Critical Context

### This is a PHP C extension — NOT PHP code

- Files ending in `.c` / `.cpp` / `.h` are compiled by `phpize` + `make`
- Tests are `.phpt` files (PHP Test Framework format), not PHPUnit
- Run tests with `make test` inside Docker, NOT `composer test` or `phpunit`
- Coverage uses `gcov` + `lcov`, NOT Xdebug or PCOV
- Build system is `config.m4` (autoconf-based)

### Function Naming

- Public PHP API: `fbird_*` prefix (e.g., `fbird_connect`, `fbird_backup`)
- Internal C helpers: `_php_fbird_*` prefix
- ibase_* aliases exist for BC — do NOT add new ibase_* functions

### Key Source Files

| File | Purpose | Lines |
|------|---------|-------|
| `firebird.c` | MINIT, globals, gen_id, batch create/add/exec, connection | 3668 |
| `fbird_query_exec.c` | SQL execution, `_php_fbird_exec()` | 1681 |
| `fbird_query_bind.c` | Parameter binding for all SQL types | 1165 |
| `fbird_service.c` | Backup, restore, user management, maintenance | 648 |
| `firebird_utils.cpp` | Batch operations C++ wrapper (FB4+ only) | 4099 |
| `fbird_query_array.c` | Firebird ARRAY type support | 544 |
| `src/cpp/fb_service.hpp` | C++ Service API wrapper | 439 |
| `php_firebird.h` | Extension header, version defines | — |
| `config.m4` | Build configuration, version detection | — |

### Common Patterns

```c
// Resource fetch with NULL check (ALWAYS required)
php_fbird_service_t *svm = zend_fetch_resource_ex(zv, le_service, iFbirdServiceType);
if (!svm) { RETURN_FALSE; }

// Error handling
_php_fbird_error(TSRMLS_C);
RETURN_FALSE;

// Version guard for FB4+ features
#if FB_API_VER >= 40
    // FB4+ specific code
#endif
```

---

## Development Constraints

### MUST (constitution Article VI)

- Feature branch only — command `git checkout -b feat/{name}` from `satware-main`
- Commit ≤ 200 lines changed per commit
- Commit format: `type(scope): description`

### MUST (constitution Article II)

- Write `.phpt` test BEFORE implementation
- Verify test FAILS (RED) before writing C code

### MUST (constitution Article VIII)

- All tests run inside Docker: `docker compose run --rm php83-dev /ext/scripts/test.sh`
- SKIPIF required for tests needing live Firebird: `--SKIPIF-- <?php if (!extension_loaded('firebird')) die('skip'); ?>`

---

## .phpt Test Structure

```phpt
--TEST--
fbird_function_name: description of what is tested
--SKIPIF--
<?php
require_once __DIR__ . '/../config.inc';
if (!extension_loaded('firebird')) die('skip firebird extension not loaded');
if (!@fbird_connect(FBIRD_TEST_DB, FBIRD_TEST_USER, FBIRD_TEST_PASS)) die('skip cannot connect to Firebird');
?>
--FILE--
<?php
require_once __DIR__ . '/../config.inc';

// Test code here
$conn = fbird_connect(FBIRD_TEST_DB, FBIRD_TEST_USER, FBIRD_TEST_PASS);
// ...
echo "ok\n";
?>
--EXPECT--
ok
```

---

## Key Scripts

| Command | What it does |
|---------|-------------|
| `docker compose run --rm php83-dev make test` | Run all .phpt tests |
| `docker compose run --rm php83-dev /ext/scripts/coverage.sh` | Generate lcov report |
| `docker compose run --rm php83-dev /ext/scripts/run-sanitizer.sh` | ASan + UBSan |
| `docker compose run --rm php83-dev /ext/scripts/run-valgrind.sh` | Valgrind leak check |
| `gh pr create --base satware-main` | Open PR |
| `gh pr merge --squash --delete-branch` | Squash-merge PR |

---

## Spec-Driven Development

For any change affecting >3 files or introducing new modules:

```text
1. Create .specify/specs/{issue}-{name}/spec.md
2. Create .specify/specs/{issue}-{name}/plan.md
3. Create .specify/specs/{issue}-{name}/tasks.md
4. Implement from tasks.md, mark as done
```

Templates: `.specify/templates/`
Constitution: `.specify/memory/constitution.md`
Project Brief: `docs/product/project-brief.md`

---

## Open Issues (v7.0.0 Milestone)

| Issue | Title | Status |
|-------|-------|--------|
| #58 | Code Coverage 80%+ gate | ✅ Closed (rc.51) |
| #59 | Service API coverage (0%→80%) | ✅ Closed (rc.51) |
| #60 | Batch operations coverage | ✅ Closed (rc.51) |
| #61 | Array operations coverage (35.9%→80%) | ✅ Closed (rc.51) |
| #62 | Parameter binding coverage (41.8%→80%) | ✅ Closed (rc.51) |
| #63 | Final coverage validation ≥80% | ✅ Closed (rc.51) |
| #66 | Release rc.51 | ✅ Released |
| #67 | Docs cleanup | ✅ Closed (merged PR #87) |
| #57 | Source refactoring (firebird.c split) | ✅ Closed |
| #68 | Doctrine integration | ✅ Closed |
| #83 | stubs: Add missing functions | ✅ Closed (merged PR #88) |
| #84 | feat(api): fbird_query_params_tx + stubs | ✅ Closed (merged PR #88) |
| #85 | docs: v7.0.0-final docs update | ✅ Closed (merged PR #87) |
| —   | v7.1.0 final release | ✅ Released |

---

## Anti-Patterns (Reject These Immediately)

- ❌ Direct PHP file as test — use `.phpt` format only
- ❌ `ibase_*` new functions — `fbird_*` prefix only
- ❌ Merging to `satware-main` directly — feature branch + PR only
- ❌ Skipping NULL check after `zend_fetch_resource_ex()`
- ❌ C++ in public extension API — C + Zend Engine only
- ❌ `#include` of `fb_*.hpp` in `.c` files — C++ wrappers are C++ only
- ❌ Hardcoded version strings — use `VERSION` file or git tag
- ❌ Tests without `--SKIPIF--` for Firebird connection
