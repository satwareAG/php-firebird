# php-firebird — Project Brief

**Version**: 7.0.0
**Last Updated**: 2026-03-02
**Status**: Active Development — v7.0.0 Release Candidate

---

## What We're Building

**php-firebird** is a native PHP C extension providing high-performance, memory-safe access to
Firebird databases from PHP 8.1–8.5. It is a modernized fork of the legacy PHP `ibase_*`
extension, renamed to `fbird_*` prefix, and maintained as a drop-in replacement with full
backwards compatibility.

### Core Value Proposition

| User | Problem | Solution |
|------|---------|---------|
| PHP developer using Firebird | No maintained native extension for PHP 8+ | php-firebird: modern, tested, packaged |
| doctrine-firebird-driver user | Extension crashes in parallel test mode | Fork-safe, memory-safe implementation |
| Distribution packager | No prebuilt binaries for modern PHP/Firebird combos | CI matrix produces binaries for all combos |
| Windows PHP developer | Old `php_interbase.dll` only works on PHP 7 | prebuilt `.dll` for PHP 8.2–8.5 NTS/TS |

---

## Technology Stack

| Layer | Technology |
|-------|-----------|
| **Language** | C (Zend Engine API), C++ (internal wrappers only) |
| **Build System** | `config.m4` (autoconf/phpize), `config.w32` (Windows) |
| **Database** | Firebird 3.0, 4.0, 5.0 (via `ibase.h` / FB API) |
| **PHP Versions** | 8.1, 8.2, 8.3, 8.4 (8.5 day-1 support) |
| **Testing** | `.phpt` files via `make test` / `run-tests.php` |
| **Coverage** | gcov + lcov (inside Docker) |
| **Sanitizers** | AddressSanitizer, UndefinedBehaviorSanitizer, Valgrind |
| **CI** | GitHub Actions (Linux + Windows matrix) |
| **Docker** | `docker/docker-compose.yml` + `docker/Dockerfile` |

---

## v7.0.0 Release Scope

### Must-Have (Release Blockers)

| Issue | Title | Status |
|-------|-------|--------|
| #69 | Extension version missing | ✅ Fixed (PR #70 merged) |
| #64 | SIGSEGV in backup/restore error paths | ✅ Fixed (PR #70 merged) |
| #58 | Code Coverage ≥80% overall | 🔄 In Progress |
| #59 | Service API coverage (fb_service.hpp 0%→80%) | 🔄 Planned |
| #60 | Batch Operations coverage (firebird_utils.cpp) | 🔄 Planned |
| #61 | Array Operations coverage (35.9%→80%) | 🔄 Planned |
| #62 | Parameter Binding coverage (41.8%→80%) | 🔄 Planned |
| #63 | Final coverage validation (≥80% overall) | 🔄 Planned |
| #57 | Source Refactoring (firebird.c split) | 🔄 Planned |
| #66 | Release v7.0.0-rc.50 tag | 🔄 Planned |

### Should-Have

| Issue | Title | Status |
|-------|-------|--------|
| #67 | Docs cleanup (docs/plans/) | 🔄 Planned |
| #68 | Doctrine integration testing | 🔄 Planned (after rc.50) |

### Not In Scope (v7.0.0)

- PECL registration (v7.0.x)
- GPG signatures (v7.0.x)
- Linux ARM64 builds (v7.0.x)
- OO API wrapper (`src/Firebird/*.php`) public promotion (v7.1.0)

---

## Architecture Overview

```text
PHP userland
    │
    ▼
fbird_*.c (Zend Engine PHP API layer)
    │ uses
    ├──→ fbird_query_exec.c   — SQL execution
    ├──→ fbird_query_bind.c   — parameter binding
    ├──→ fbird_query_array.c  — array type support
    ├──→ fbird_query_prepare.c — prepared statements
    ├──→ fbird_service.c      — backup/restore/admin
    ├──→ fbird_blobs.c        — BLOB streaming
    ├──→ fbird_events.c       — async event listener
    ├──→ fbird_result.c       — result set fetching
    ├──→ fbird_metadata.c     — schema inspection
    ├──→ fbird_datetime.c     — date/time handling
    └──→ firebird.c           — MINIT, globals, gen_id, etc.
    │ uses (C++ internal wrappers, FB4+ only)
    └──→ firebird_utils.cpp   — Batch API (FB4+)
    └──→ src/cpp/*.hpp        — OO C++ wrappers (not public)
    │
    ▼
Firebird client library (libfbclient.so / fbclient.dll)
    │
    ▼
Firebird server (3.0 / 4.0 / 5.0)
```

---

## Development Workflow

```text
1. Create spec in .specify/specs/{issue}-{name}/spec.md
2. Get spec reviewed/approved
3. Create plan.md + tasks.md
4. Implement on feature branch (never satware-main)
5. Tests RED → GREEN → REFACTOR
6. Coverage ≥80% in Docker
7. Sanitizers clean
8. PR → CI green → squash merge
```

---

## Key Files for New Contributors

| File | Purpose |
|------|---------|
| `README.md` | User-facing installation and usage |
| `CONTRIBUTING.md` | Contribution process |
| `docker/Dockerfile` | Test environment definition |
| `scripts/test.sh` | Run all .phpt tests |
| `scripts/coverage.sh` | Generate lcov coverage report |
| `scripts/run-sanitizer.sh` | ASan/UBSan test run |
| `tests/common.inc` | Shared test helpers |
| `tests/config.inc` | Test DB connection config |
| `.specify/memory/constitution.md` | Project principles (read first) |

---

## v7.0.0 Success Criteria

- [ ] All existing 135+ tests pass on PHP 8.1–8.4 × Firebird 3.0–5.0
- [ ] Overall code coverage ≥ 80% (lcov)
- [ ] Zero ASan/UBSan/Valgrind errors on new code
- [ ] `firebird.c` split into logical modules (<1500 lines each)
- [ ] Doctrine-firebird-driver test suite passes against rc.50
- [ ] `v7.0.0-rc.50` tagged and GitHub release published
- [ ] Windows DLLs built for PHP 8.2–8.5 NTS/TS
