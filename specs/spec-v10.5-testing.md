---
description: >-
  v10.5.0 testing improvements: --CLEAN-- sections for .phpt tests,
  TSan for ZTS builds, fuzz dictionary, macOS CI job.
tags: [testing, fuzzing, tsan, macos, v10.5]
priority: 7
---

> **Status: OPEN** - Milestone v10.5.0

# Spec: v10.5.0 Testing Improvements

## Goal

Strengthen test infrastructure with proper cleanup sections, thread safety
validation, improved fuzzing, and macOS build verification.

## Success Criteria

- [ ] H6: All .phpt tests creating DB objects have `--CLEAN--` sections
- [ ] M5: TSan job in sanitizers.yml with ZTS PHP build
- [ ] M6: Fuzz dictionary with Firebird SQL keywords in `fuzz/dictionary/sql.dict`
- [ ] M11: macOS CI job (build-only) in ci.yml
- [ ] No test interdependencies on failed test artifacts

## Part 1: H6 - PHPT --CLEAN-- Sections

Add `--CLEAN--` sections to all .phpt tests that create tables, databases,
or other persistent artifacts. Start with 40 coverage tests and 45 PDO tests,
then extend to remaining tests.

### Affected Files

- `tests/coverage/*.phpt` (~40 files)
- `tests/pdo_fbird/*.phpt` (~45 files)
- Remaining `tests/*.phpt` as needed

## Part 2: M5 - TSan for ZTS Builds

Add Thread Sanitizer job to `sanitizers.yml` using a ZTS-enabled PHP build.
TSan detects data races in `IBG()` global state access and `master_instance`.

### Affected Files

- `.github/workflows/sanitizers.yml`
- `docker/Dockerfile` (ZTS build variant)

## Part 3: M6 - Fuzz Dictionary

Create `fuzz/dictionary/sql.dict` with Firebird SQL keywords, operators, and
special characters to improve mutation-based fuzzing coverage.

### Affected Files

- `fuzz/dictionary/sql.dict` (new)
- `fuzz/run.php` - load dictionary option

## Part 4: M11 - macOS CI

Add macOS runner to `ci.yml` for build-only verification (no Firebird server
available on GitHub macOS runners). Test Apple Silicon (ARM64) compatibility.

### Affected Files

- `.github/workflows/ci.yml` - macOS job

## Test Strategy

1. Verify `--CLEAN--` sections execute on test failure (intentional fail test)
2. TSan job reports zero data races on test suite
3. Fuzzer with dictionary finds more code paths than without
