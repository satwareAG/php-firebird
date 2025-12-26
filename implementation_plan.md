# Implementation Plan: AddressSanitizer (ASan) Integration

## Goal
Enable robust AddressSanitizer (ASan) testing for the `php-firebird` extension by creating a custom PHP build environment with ASan enabled, bypassing the `RTLD_DEEPBIND` conflict inherent in stock PHP binaries.

## Problem Analysis
- **Current State:** `scripts/analysis/sanitizers.sh` attempts to use `LD_PRELOAD` with stock PHP binaries.
- **Issue:** PHP loads extensions with `RTLD_DEEPBIND`, which conflicts with ASan's requirement to be the first loaded library (or preloaded). This causes immediate crashes.
- **Solution:** Build PHP from source with ASan enabled statically or linked correctly. This ensures the PHP binary itself is ASan-aware and handles memory correctly, allowing the extension (also built with ASan) to be tested effectively.

## Steps

### 1. Research & Configuration
- [ ] Identify exact `configure` flags for building PHP with ASan.
  - Likely `CFLAGS="-fsanitize=address -fno-omit-frame-pointer" LDFLAGS="-fsanitize=address"`.
  - Verify if `--enable-debug` is required or recommended.
  - Check for any known issues with specific PHP versions (targeting 8.3 as primary).

### 2. Docker Environment
- [ ] Create `docker/php/Dockerfile-asan`.
  - Base image: `debian:bookworm-slim` or similar (to match existing `php83-dev` base).
  - Install build dependencies (compilers, libxml2, sqlite3, etc.).
  - Download and compile PHP 8.3 source with ASan flags.
  - Configure it to support the Firebird extension build (install `firebird-dev`).
- [ ] Update `docker/docker-compose.yml`.
  - Add a `php83-asan` service using the new Dockerfile.

### 3. Script Updates
- [ ] Update `scripts/analysis/sanitizers.sh`.
  - Detect if running in the ASan container.
  - If in ASan container, skip `LD_PRELOAD` logic and run tests directly.
  - If not in ASan container, warn or fail (or fallback if possible, though unlikely to work).
- [ ] Update `scripts/qa.sh`.
  - Add an option or step to run sanitizer tests using the `php83-asan` container.
- [ ] Update `scripts/run-sanitizer.sh`.
  - Ensure it targets the `php83-asan` container.

### 4. Verification
- [ ] Build the new Docker image.
- [ ] Run the sanitizer tests.
- [ ] Verify that ASan is active (check for ASan banner or introduce a deliberate leak/bug in `firebird.c` temporarily to confirm detection).

## Success Criteria
- `scripts/analysis/sanitizers.sh` runs without crashing due to `RTLD_DEEPBIND`.
- ASan reports memory errors if present.
- Normal tests pass in the ASan environment (no false positives from PHP itself).
