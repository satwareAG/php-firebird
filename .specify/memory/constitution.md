# php-firebird Project Constitution

**Version**: 1.0.0
**Date**: 2026-03-02
**Status**: ACTIVE — All development decisions governed by these principles

---

## Preamble

php-firebird is a PHP C extension providing native Firebird database access. It is a
community-maintained fork of the legacy PHP `ibase_*` extension, modernized for PHP 8.x
and Firebird 3.0–5.0. Every contribution MUST uphold these non-negotiable principles.

---

## Article I: C Extension First

- **MUST**: All public PHP API functions (`fbird_*`) are implemented in C using the Zend Engine API
- **MUST**: C++ (`src/cpp/*.hpp`) is used ONLY for internal OO wrappers, never exposed directly
- **MUST**: All new `.c` files MUST be registered in `config.m4` (Linux) AND `config.w32` (Windows)
- **SHALL NOT**: Use PHP-level code as a substitute for C extension functionality

*Rationale*: Users expect zero-overhead native extension performance, not a PHP wrapper layer.

---

## Article II: Test-First Imperative

This is NON-NEGOTIABLE.

- **MUST**: Every implementation change has a corresponding `.phpt` test written BEFORE the change
- **MUST**: Tests MUST fail (RED) before implementation, pass (GREEN) after
- **MUST**: No PR merges without tests covering the changed code paths
- **MUST**: Edge cases (NULL inputs, error paths, boundary values) are tested explicitly
- **SHALL NOT**: Write implementation code before a failing test exists

*Rationale*: C extension bugs cause SIGSEGV and heap corruption. Tests are the safety net.

---

## Article III: Memory Safety

- **MUST**: Every code path passes AddressSanitizer (ASan) without errors
- **MUST**: Every code path passes UndefinedBehaviorSanitizer (UBSan) without errors
- **MUST**: Valgrind leak check shows no definite leaks on new code
- **MUST**: All resource handles checked for NULL after `zend_fetch_resource_ex()`
- **MUST**: Destructors guard against double-free with handle validity checks
- **SHALL NOT**: Dereference a pointer without NULL check when it comes from user input

*Rationale*: Memory errors in PHP extensions crash the entire PHP process.

---

## Article IV: API Stability

- **MUST**: All public functions use the `fbird_*` prefix
- **MUST**: `ibase_*` aliases are **removed** as of v7.2.0 — deprecated in v7.1.0, no longer present in the extension
- **MUST**: No parameter signature changes to existing `fbird_*` functions without deprecation notice
- **SHALL**: New functions follow the naming pattern `fbird_{noun}_{verb}` (e.g., `fbird_service_backup`)
- **SHALL NOT**: Remove or rename existing `fbird_*` functions in a minor release
- **SHALL NOT**: Add new `ibase_*` symbols — `fbird_*` prefix only

*Rationale*: Downstream projects (doctrine-firebird-driver, legacy apps) depend on API stability.

---

## Article V: Multi-Version Compatibility

- **MUST**: Extension compiles and tests pass on PHP 8.2, 8.3, 8.4
- **MUST**: Extension compiles and tests pass on Firebird 3.0, 4.0, and 5.0 (all actively supported)
- **MUST**: Firebird 4.0+ features guarded with `#if FB_API_VER >= 40`
- **MUST**: Tests for FB4+ features include `--SKIPIF--` with server version check
- **SHALL NOT**: Use PHP version-specific APIs without a compatibility shim

*Rationale*: The CI matrix covers PHP 8.2/8.3/8.4 × Firebird 3.0/4.0/5.0 (9 combinations).
All three Firebird versions are actively supported as of 2026. Each client version must be
tested independently because `FB_API_VER` determines which code paths compile.

---

## Article VI: Atomic Commits

- **MUST**: Each commit addresses exactly one logical concern
- **MUST**: Commit size ≤ 200 lines changed (additions + deletions)
- **MUST**: All development on feature branches — never direct commits to `satware-main`
- **MUST**: Commit message format: `type(scope): description` (feat, fix, test, refactor, docs, chore)
- **SHALL NOT**: Mix refactoring with feature additions in a single commit

*Rationale*: Small commits make bisect, review, and rollback practical.

---

## Article VII: Coverage Gate

- **MUST**: Overall test coverage ≥ 80% before v7.0.0-final release
- **MUST**: Any new file achieves ≥ 80% coverage before its PR merges
- **MUST**: Coverage measured with gcov/lcov inside Docker (`scripts/coverage.sh`)
- **SHALL**: Coverage reports stored in `tests/coverage/` directory
- **SHALL NOT**: Merge PRs that drop coverage below the current baseline

*Rationale*: Coverage below 80% means critical paths are untested in production.

---

## Article VIII: Docker-Native Testing

- **MUST**: All tests runnable via `docker compose run --rm php83-dev /ext/scripts/test.sh`
- **MUST**: Tests requiring a live Firebird server use `--SKIPIF--` with connection check
- **MUST**: New Docker-dependent test infrastructure is documented in `docker/README.md`
- **SHALL**: Coverage generation runs exclusively inside Docker (`scripts/coverage.sh`)
- **SHALL NOT**: Assume local Firebird installation in test `--SKIPIF--` conditions

*Rationale*: Reproducible test environment across developer machines and CI.

---

## Article IX: Specification-Driven Development

- **MUST**: Complex changes (>3 files, new modules, architectural changes) have a spec before implementation
- **MUST**: Specs live in `.specify/specs/{issue-number}-{feature-name}/`
- **MUST**: Each spec has `spec.md` (what/why), `plan.md` (how), `tasks.md` (action items)
- **SHALL**: Specs reviewed and approved before implementation begins
- **SHALL NOT**: Implement architectural changes without a reviewed spec

*Rationale*: Spec-first development prevents scope creep, documents intent, and enables parallel work.

---

## Enforcement

These articles are enforced via:
1. **CI/CD**: GitHub Actions matrix (PHP 8.1–8.5 × Firebird 3.0–5.0, ASan, UBSan, LeakSan, Coverage)
2. **Pre-commit hooks**: `scripts/pre-commit-hook.sh` (gitleaks, clang-tidy)
3. **PR policy**: All PRs require at least one CI green run before merge
4. **Code review**: Maintainers verify Article compliance during review

No exceptions without explicit documented rationale.
