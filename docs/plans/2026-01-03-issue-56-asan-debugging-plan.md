# Implementation Plan: Issue #56 Heap Corruption ASAN Debugging

## [Overview]
Systematic debugging approach for the doctrine-firebird-driver heap corruption issue using the project's comprehensive sanitizer infrastructure.

The doctrine-firebird-driver team reports heap corruption in production (PHP 8.4, Ubuntu 24.04), which crashes at random points. The key discovery is that `USE_ZEND_ALLOC=0` eliminates the crash, suggesting the bug involves interaction between the extension's memory management and Zend Memory Manager.

This plan uses the php83-asan container (PHP built with AddressSanitizer from source) to detect memory errors with precise stack traces. The ASAN container already has `USE_ZEND_ALLOC=0` set, which means:
1. If ASAN finds errors → We get exact location of the bug
2. If ASAN passes cleanly → Bug is specifically in Zend MM interaction (timing/allocation pattern dependent)

## [Types]
No new types needed. Uses existing test infrastructure types.

## [Files]
Files to be created or modified for this debugging effort.

**New Files:**
- `tests/issue56_connection_stress.phpt` - PHPT test mimicking doctrine-firebird-driver pattern
- `tests/sanitizer/issue56_doctrine_pattern.php` - ASan test for multiple connection/query cycles
- `docs/learnings/2026-01-03-issue-56-asan-findings.md` - Documentation of findings

**Existing Files to Reference:**
- `scripts/qa.sh` - ASAN mode runner (`--asan` flag)
- `scripts/analysis/sanitizers.sh` - Detailed sanitizer options
- `scripts/analysis/valgrind.sh` - Memory analysis fallback
- `docker/php/Dockerfile-asan` - PHP 8.3.14 with ASAN baked in
- `docker/docker-compose.yml` - php83-asan container configuration
- `tests/sanitizer/*.php` - Existing sanitizer test patterns

## [Functions]
No new functions needed. Testing existing functions under sanitizer instrumentation.

**Functions Under Investigation:**
- `_php_fbird_close_link()` - Connection destructor
- `fbc_disconnect()` - C++ wrapper disconnect
- `ConnectionWrapper::detach()` - C++ RAII cleanup
- `ServiceWrapper::detach()` - Service API cleanup
- `_php_fbird_free_result_impl()` - Result set cleanup
- Zend resource destructors for all resource types

## [Classes]
No new classes needed.

**C++ Wrappers Under Investigation:**
- `ConnectionWrapper` (src/cpp/fb_connection.hpp)
- `TransactionWrapper` (src/cpp/fb_transaction.hpp)
- `StatementWrapper` (src/cpp/fb_statement.hpp)
- `ServiceWrapper` (src/cpp/fb_service.hpp)
- `BlobWrapper` (src/cpp/fb_blob.hpp)

## [Dependencies]
No new dependencies.

**Existing Dependencies:**
- Docker Compose environment with php83-asan container
- Firebird 4.0+ server (firebird40 service)
- AddressSanitizer runtime (built into php83-asan container)

## [Testing]
Multi-phase testing approach using sanitizer infrastructure.

**Phase 1: Run Full Test Suite with ASAN**
```bash
./scripts/qa.sh --asan
```
Expected: All 135 tests pass, or ASAN reports specific errors

**Phase 2: Run Targeted Sanitizer Tests**
```bash
# Inside php83-asan container
php -d extension=./modules/firebird.so tests/sanitizer/issue56_doctrine_pattern.php
```

**Phase 3: Compare with Valgrind**
```bash
./scripts/qa.sh --valgrind
```

**Phase 4: Run PHPT Test Under ASAN**
```bash
# Run new PHPT test specifically designed to trigger the issue pattern
TEST_PHP_EXECUTABLE=php php run-tests.php tests/issue56_connection_stress.phpt
```

## [Implementation Order]
Systematic Baby Steps approach for debugging.

1. **Verify ASAN Environment** - Run `./scripts/qa.sh --asan` to confirm setup works
2. **Analyze ASAN Output** - Check for any memory errors in existing test suite
3. **Create Doctrine Pattern Test** - Write test mimicking doctrine-firebird-driver usage
4. **Run Pattern Test Under ASAN** - Execute new test with sanitizer
5. **Analyze Stack Traces** - If errors found, identify exact source location
6. **Cross-Reference with Valgrind** - Run same tests under Valgrind for confirmation
7. **Document Findings** - Create learnings document with results
8. **Implement Fix (if error found)** - Baby Steps fix with test
9. **Update Issue #56** - Post findings/fix to GitHub issue

## [Debugging Tools Quick Reference]

### ASAN Container Environment
```bash
# Environment variables preset in Dockerfile-asan:
USE_ZEND_ALLOC=0              # Bypass Zend MM for ASAN visibility
ZEND_DONT_UNLOAD_MODULES=1    # Keep symbols for stack traces
ASAN_OPTIONS="exitcode=139:abort_on_error=0:detect_leaks=1:check_initialization_order=1:strict_init_order=1:detect_stack_use_after_return=1:halt_on_error=0"
```

### Running ASAN Tests
```bash
# Full test suite with ASAN
./scripts/qa.sh --asan

# Manual container entry
cd docker && docker compose up -d php83-asan firebird40
docker compose exec php83-asan bash
cd /ext
phpize && ./configure --with-firebird=/usr && make -j$(nproc)
php -d extension=./modules/firebird.so tests/sanitizer/asan_basic.php
```

### Valgrind Fallback
```bash
# Full Valgrind analysis
./scripts/qa.sh --valgrind

# Quick Valgrind test
./scripts/qa.sh --valgrind --container php83-dev
```

### UBSan (Undefined Behavior)
```bash
# Inside any dev container
/ext/scripts/analysis/sanitizers.sh ubsan
```

## [Expected Outcomes]

**Scenario A: ASAN detects error**
- Get precise stack trace with source file, line number, and allocation history
- Implement targeted fix
- Add regression test
- Close Issue #56 with fix

**Scenario B: All sanitizers pass**
- Document that memory management is correct at the extension level
- Bug is in Zend MM timing/interaction (race condition, allocation pattern)
- Propose workaround: `USE_ZEND_ALLOC=0` in production
- Request minimal reproduction case from doctrine-firebird-driver team
- Consider bisecting PHP versions to find Zend MM change

**Scenario C: Intermittent detection**
- Run stress tests with multiple iterations
- Add deliberate delays to trigger race conditions
- Use `detect_stack_use_after_return=1` to catch stack use-after-free
