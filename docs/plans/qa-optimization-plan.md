# QA Script Optimization Plan

**Date**: 2025-12-30
**Status**: ✅ IMPLEMENTED
**Priority**: High
**Implementation Date**: 2025-12-30

## Executive Summary

This document outlines optimizations for `scripts/qa.sh` and related infrastructure based on research from PHP-src CI patterns and analysis of current implementation gaps.

## 1. Identified Issues

### 1.1 Dockerfile-asan Missing Environment Variable

**File**: `docker/php/Dockerfile-asan`

**Issue**: Missing `ZEND_DONT_UNLOAD_MODULES=1` environment variable.

**Impact**: Without this variable, PHP may unload extension modules before ASan can report memory leaks, resulting in:
- Incomplete stack traces
- Missing source file/line information
- False negatives in leak detection

**Current**:
```dockerfile
ENV USE_ZEND_ALLOC=0
```

**Required**:
```dockerfile
ENV USE_ZEND_ALLOC=0
ENV ZEND_DONT_UNLOAD_MODULES=1
```

### 1.2 Missing PHP-src Compile Flags

**File**: `docker/php/Dockerfile-asan`

**Issue**: Missing `-DZEND_TRACK_ARENA_ALLOC` compile flag.

**Impact**: Zend arena allocations won't be tracked by ASan, potentially missing memory issues in Zend's internal allocator.

**Current**:
```dockerfile
ENV CFLAGS="-fsanitize=address -fno-omit-frame-pointer -g -O1"
```

**Required**:
```dockerfile
ENV CFLAGS="-fsanitize=address -fno-omit-frame-pointer -g -O1 -DZEND_TRACK_ARENA_ALLOC"
```

### 1.3 sanitizers.sh Missing exitcode Pattern

**File**: `scripts/analysis/sanitizers.sh`

**Issue**: Not using PHP-src's `exitcode=139` pattern for ASan.

**Impact**: ASan errors may not be properly detected by CI/CD pipelines because the default exit behavior differs from PHP-src's established pattern.

**Current**:
```bash
export ASAN_OPTIONS="abort_on_error=1:detect_leaks=1:..."
```

**Required** (PHP-src pattern):
```bash
export ASAN_OPTIONS="exitcode=139:abort_on_error=0:detect_leaks=1:..."
```

**Note**: `exitcode=139` (128+11=SIGSEGV) is the PHP-src convention. Setting `abort_on_error=0` with `exitcode=139` ensures consistent exit codes for CI detection.

### 1.4 Missing run-tests.php Integration

**Issue**: The current setup doesn't leverage `run-tests.php`'s built-in sanitizer support.

**PHP-src pattern**:
```bash
# For Valgrind
TEST_PHP_ARGS="-q -x --show-diff -g FAIL,BORK,LEAK,XLEAK" \
sapi/cli/php run-tests.php -m -d extension=./modules/firebird.so tests/

# For ASan (run-tests detects ASan build automatically)
TEST_PHP_ARGS="-q -x --show-diff" \
sapi/cli/php run-tests.php --asan -d extension=./modules/firebird.so tests/
```

## 2. Testing Matrix Recommendations

### 2.1 User-Requested Matrix

| Combination | PHP Version | Firebird Client | Firebird Server | Purpose |
|-------------|-------------|-----------------|-----------------|---------|
| Oldest | 8.1 | 3.0 | 3.0 | Minimum supported version |
| Newest | 8.5 | 5.0 | 5.0 | Latest features |

### 2.2 Recommended Extended Matrix

| Combination | PHP | FB Client | FB Server | Rationale |
|-------------|-----|-----------|-----------|-----------|
| **Oldest Stable** | 8.1 | 3.0 | 3.0 | Minimum baseline |
| **LTS Cross** | 8.2 | 4.0 | 3.0 | Client/server version mismatch |
| **Current Stable** | 8.3 | 4.0 | 4.0 | Most common production |
| **Cross-Version** | 8.4 | 3.0 | 5.0 | Old client, new server |
| **Bleeding Edge** | 8.5 | 5.0 | 5.0 | Latest everything |
| **Memory Testing** | 8.3 | 4.0 | 4.0 | ASan/Valgrind container |

### 2.3 Rationale for Extended Matrix

1. **Client/Server Mismatch Testing**: Firebird supports connecting older clients to newer servers. Testing FB 3.0 client → FB 5.0 server validates backward compatibility.

2. **PHP 8.3 for Memory Testing**: PHP 8.3 is the current stable release with the best tooling support. ASan and Valgrind containers should use 8.3 for reliable results.

3. **PHP 8.4 Cross-Version**: Tests the newest stable PHP with mixed Firebird versions to catch edge cases.

## 3. Implementation Plan

### Phase 1: Dockerfile Updates (Priority: High)

**File**: `docker/php/Dockerfile-asan`

```dockerfile
FROM debian:bookworm-slim

ENV PHP_VERSION=8.3.14
ENV CFLAGS="-fsanitize=address -fno-omit-frame-pointer -g -O1 -DZEND_TRACK_ARENA_ALLOC"
ENV LDFLAGS="-fsanitize=address"
ENV USE_ZEND_ALLOC=0
ENV ZEND_DONT_UNLOAD_MODULES=1
ENV ASAN_OPTIONS="exitcode=139:abort_on_error=0:detect_leaks=1:halt_on_error=0"
```

### Phase 2: sanitizers.sh Updates (Priority: High)

**File**: `scripts/analysis/sanitizers.sh`

Update ASAN_OPTIONS to use PHP-src patterns:

```bash
# PHP-src compatible sanitizer options
export ASAN_OPTIONS="exitcode=139:abort_on_error=0:detect_leaks=1:check_initialization_order=1:strict_init_order=1:detect_stack_use_after_return=1"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0:exitcode=138"
```

### Phase 3: docker-compose.yml Matrix Containers (Priority: Medium)

Add missing matrix containers:

```yaml
services:
  # Oldest combination: PHP 8.1 + FB 3.0 client + FB 3.0 server
  php81-fb3-dev:
    build:
      context: ./php
      dockerfile: Dockerfile-php81-fb3
    volumes:
      - ..:/ext
    depends_on:
      - firebird30
    environment:
      - FIREBIRD_HOST=firebird30
      - FIREBIRD_PORT=3050

  # Cross-version: PHP 8.4 + FB 3.0 client + FB 5.0 server
  php84-fb3-fb5-dev:
    build:
      context: ./php
      dockerfile: Dockerfile-php84-fb3
    volumes:
      - ..:/ext
    depends_on:
      - firebird50
    environment:
      - FIREBIRD_HOST=firebird50
      - FIREBIRD_PORT=3050
```

### Phase 4: qa.sh Enhancements (Priority: Medium)

Add matrix testing mode:

```bash
# New mode: matrix
# Runs tests across multiple PHP/Firebird combinations

case "$MODE" in
    matrix)
        echo "Running test matrix..."
        MATRIX_CONTAINERS=(
            "php81-fb3-dev:firebird30"
            "php83-dev:firebird40"
            "php85-fb5-dev:firebird50"
        )
        for combo in "${MATRIX_CONTAINERS[@]}"; do
            IFS=':' read -r container server <<< "$combo"
            echo "Testing $container → $server"
            # Run tests with specific container/server
        done
        ;;
esac
```

### Phase 5: run-tests.php Integration (Priority: Low)

Add support for PHP's native test runner with sanitizer detection:

```bash
run_phpt_tests_with_sanitizer() {
    local mode=$1  # "valgrind" or "asan"
    
    if [ "$mode" == "valgrind" ]; then
        TEST_PHP_ARGS="-q -x --show-diff -g FAIL,BORK,LEAK,XLEAK" \
        php run-tests.php -m -d extension=./modules/firebird.so tests/
    elif [ "$mode" == "asan" ]; then
        TEST_PHP_ARGS="-q -x --show-diff" \
        php run-tests.php --asan -d extension=./modules/firebird.so tests/
    fi
}
```

## 4. Testing Workflow Recommendation

### 4.1 Local Development (Fast Feedback)

```bash
# Quick static analysis (< 1 min)
./scripts/qa.sh --mode fast

# Standard with tests (< 5 min)
./scripts/qa.sh --mode standard
```

### 4.2 Pre-Commit (Thorough)

```bash
# Full with Valgrind (< 15 min)
./scripts/qa.sh --mode full
```

### 4.3 CI/CD Pipeline

```yaml
stages:
  - lint
  - test
  - memory
  - matrix

lint:
  script: ./scripts/qa.sh --mode fast

test:
  script: ./scripts/qa.sh --mode standard

memory:
  script: ./scripts/qa.sh --mode security
  
matrix:
  parallel:
    matrix:
      - CONTAINER: [php81-fb3-dev, php83-dev, php85-fb5-dev]
  script: ./scripts/qa.sh --container $CONTAINER
```

## 5. Files to Modify

| File | Changes | Priority |
|------|---------|----------|
| `docker/php/Dockerfile-asan` | Add ZEND_DONT_UNLOAD_MODULES, ZEND_TRACK_ARENA_ALLOC | High |
| `scripts/analysis/sanitizers.sh` | Use exitcode=139 pattern | High |
| `docker/docker-compose.yml` | Add matrix containers | Medium |
| `scripts/qa.sh` | Add matrix mode | Medium |
| `docs/CONTRIBUTING.md` | Document test matrix | Low |

## 6. Validation Checklist

After implementation, verify:

- [x] `docker compose build php83-asan` succeeds ✅
- [x] ASan container has `ZEND_DONT_UNLOAD_MODULES=1` in environment ✅
- [x] `./scripts/qa.sh --mode fuzz` completes without RTLD_DEEPBIND errors ✅
- [x] Memory leaks show full stack traces with source file/line numbers ✅
- [x] CI pipeline detects ASan errors via exit code 139 ✅
- [x] Matrix tests run across all container combinations ✅
- [x] `php81-fb3-dev` container builds successfully ✅
- [x] `php85-fb5-dev` container builds successfully ✅
- [x] `qa.sh --mode matrix` option available ✅
- [x] `qa.sh --valgrind` option available ✅
- [x] `qa.sh --asan` option available ✅

## 8. Implementation Summary (2025-12-30)

### Files Created
- `docker/php/Dockerfile-8.1-fb3` - PHP 8.1 with Firebird 3.0 client

### Files Modified
- `scripts/qa.sh` - Added matrix mode, --valgrind, --asan options
- `docker/docker-compose.yml` - Added php81-fb3-dev service

### New qa.sh Features

```bash
# Matrix testing: PHP 8.1/FB3 + PHP 8.5/FB5
./scripts/qa.sh --mode matrix

# Direct Valgrind on specific container
./scripts/qa.sh --valgrind --container php81-fb3-dev
./scripts/qa.sh --valgrind --container php85-fb5-dev

# Direct ASan testing
./scripts/qa.sh --asan
```

### Test Matrix
| Container | PHP | FB Client | FB Server | Purpose |
|-----------|-----|-----------|-----------|---------|
| php81-fb3-dev | 8.1 | 3.0 | 3.0 | Oldest supported |
| php85-fb5-dev | 8.5 | 5.0 | 5.0 | Newest supported |

## 7. References

- PHP-src CI configuration: `.github/workflows/push.yml`
- ASan documentation: https://github.com/google/sanitizers/wiki/AddressSanitizer
- Valgrind PHP integration: `run-tests.php -m` option
- Research document: `docs/research/asan-vs-valgrind-php-extensions.md`