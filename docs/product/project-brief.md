# php-firebird — Project Brief

**Version**: 7.2.0
**Last Updated**: 2026-03-04
**Status**: Stable — v7.2.0 Released

---

## What We're Building

**php-firebird** is a native PHP C extension providing high-performance, memory-safe access to
Firebird databases from PHP 8.2+. It is a modernized fork of the legacy PHP `ibase_*`
extension, renamed to `fbird_*` prefix. The `ibase_*` aliases were fully removed in v7.2.0.

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
| **PHP Versions** | 8.2, 8.3, 8.4 (8.5 day-1 support planned) |
| **Testing** | `.phpt` files via `make test` / `run-tests.php` |
| **Coverage** | gcov + lcov (inside Docker) |
| **Sanitizers** | AddressSanitizer, UndefinedBehaviorSanitizer, Valgrind |
| **CI** | GitHub Actions (Linux + Windows matrix) |
| **Docker** | `docker/docker-compose.yml` + `docker/Dockerfile` |

---

## Release History

| Version | Date | Highlights |
|---------|------|-----------|
| **7.2.0** | 2026-03-04 | Drop PHP 8.1, Firebird 2.5, `ibase_*` aliases removed |
| 7.1.0 | 2026-03-04 | `fbird_query_params_tx()`, deprecation warnings for v7.2.0 changes |
| 7.0.0 | 2026-02-xx | Major refactor: source split, coverage ≥80%, sanitizer-clean |

---

## v7.2.0 Breaking Changes

| Change | Details |
|--------|---------|
| **PHP ≥8.2 required** | PHP 8.1 EOL Nov 2025 — removed from CI and composer.json |
| **Firebird ≥3.0 required** | Firebird 2.5 EOL Sep 2020 — removed from Docker/CI |
| **`ibase_*` removed** | All `PHP_FALIAS` entries gone — use `fbird_*` exclusively |

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

## v7.2.0 Success Criteria (All Met ✅)

- [x] All tests pass on PHP 8.2–8.4 × Firebird 3.0–5.0
- [x] Overall code coverage ≥ 80% (lcov)
- [x] Zero ASan/UBSan/Valgrind errors
- [x] `ibase_*` aliases fully removed
- [x] PHP 8.1 support removed from CI and composer.json
- [x] Firebird 2.5 removed from Docker/CI
- [x] `v7.2.0` tagged and GitHub Release published
- [x] Windows DLLs built for PHP 8.2–8.5 NTS/TS
- [x] Stubs `v7.2.0` tagged on Packagist

---

## Next Development Focus (v7.3.0 / doctrine-firebird-driver)

- doctrine-firebird-driver DBAL 4.x forward-compatibility (issue #78)
- Simplify test suite now PHP 8.1 is dropped (issue #47 in doctrine repo)
- v3.11.0 release: Configurable LIKE CAST Length (issue #20 in doctrine repo)
- See `NEXT_STEPS.md` for full backlog
