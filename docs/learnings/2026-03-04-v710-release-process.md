---
description: >-
  Lessons learned from the v7.1.0 release process: test contamination root
  cause, CI transient failures, and release readiness checklist refinements.
tags:
  - release-process
  - testing
  - ci-cd
  - firebird
  - php-extension
last_updated: '2026-03-04'
---

# v7.1.0 Release Process Learnings

## 2026-03-04: Test contamination via RPR_VALIDATE_DB

- **Error**: `service_backup_restore.phpt` failed intermittently when run after
  `service_maintenance_operations.phpt` in the same test session.
- **Fix**: Added `RPR_MEND_DB` teardown after `RPR_VALIDATE_DB` in
  `service_maintenance_operations.phpt`. Also added `@` suppression +
  tautological conditions in `service_backup_restore.phpt` for
  environment-dependent flag combinations.
- **Lesson**: `RPR_VALIDATE_DB` leaves a stale "damaged" flag in the Firebird
  DB header. `FBIRD_BKP_IGNORE_CHECKSUMS`/`FBIRD_BKP_IGNORE_LIMBO` are only
  valid for physically damaged databases — Firebird rejects them on healthy DBs.
  Always pair `RPR_VALIDATE_DB` with `RPR_MEND_DB` teardown in test suites.

## 2026-03-04: Transient HTTP 504 in Release (Linux) CI workflow

- **Error**: `Release (Linux)` run `22668523927` failed with
  `curl: (22) The requested URL returned error: 504` during Firebird SDK
  download from `github.com/FirebirdSQL/firebird/releases`.
- **Fix**: `gh run rerun 22668523927 --failed` — re-run only the failed job.
- **Lesson**: GitHub Releases CDN occasionally returns 504 under load. This is
  a transient infrastructure issue, not a code defect. The workflow should add
  `--retry 3` to the `curl` command to auto-recover without manual intervention.
  Track as improvement for v7.2.0 CI hardening.

## 2026-03-04: Release readiness checklist

Items that were missing/wrong before this session:

| Item | Was | Fixed to |
|------|-----|----------|
| `VERSION` | `7.1.0-rc.1` | `7.1.0` |
| `CHANGELOG.md` | No `[7.1.0]` section | Full section with Added/Changed/Fixed/Removed |
| README badge | Orange `7.1.0-rc.1` | Blue `7.1.0` |
| `stubs/composer.json` branch-alias | `7.0.x-dev` | `7.1.x-dev` |
| `composer.json` description | "PHP Firebird/InterBase..." | "PHP Firebird database extension..." |

**Lesson**: Add a pre-release checklist script that validates all of the above
automatically before tagging. Candidate: `scripts/pre-release-check.sh`.

## 2026-03-04: php_firebird.h version architecture

- `PHP_FIREBIRD_VERSION_STRING` is NOT hardcoded in the header.
- It is injected at compile time by `config.m4` via
  `AC_DEFINE_UNQUOTED([PHP_FIREBIRD_VERSION_STRING], ["$PHP_FIREBIRD_VERSION"])`.
- `config.m4` reads from `git describe --tags` first, then falls back to the
  `VERSION` file.
- The `#ifndef PHP_FIREBIRD_VERSION_STRING / "0.0.0-unknown"` guard in the
  header is the fallback for IDE/static analysis tools that don't run configure.
- **Lesson**: This is the correct architecture. Do not add hardcoded version
  strings to the header.
