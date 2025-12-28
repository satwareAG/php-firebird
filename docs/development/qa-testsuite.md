# QA Testsuite Reference

Comprehensive quality assurance pipeline for the php-firebird extension.

## Quick Start

```bash
# Standard QA (recommended before commits)
./scripts/qa.sh

# Fast mode - static analysis only
./scripts/qa.sh --mode fast

# Full mode - includes sanitizers and Valgrind
./scripts/qa.sh --mode full

# Test specific PHP version
./scripts/qa.sh --container php84-dev

# Run specific tests across all PHP versions
./scripts/test_matrix.sh "" "" tests/fbird_blob_001.phpt
```

## Scripts Overview

| Script | Purpose | Duration |
|--------|---------|----------|
| `scripts/qa.sh` | Main QA orchestrator (6-phase) | 2-15 min |
| `scripts/test_matrix.sh` | Multi-version PHP testing | 10-30 min |
| `scripts/test.sh` | PHPT test runner | 2 min |
| `scripts/build.sh` | Extension build | 1 min |
| `scripts/analysis/clang_tidy.sh` | C++ static analysis | 30 sec |
| `scripts/analysis/cppcheck.sh` | C bug detection | 30 sec |
| `scripts/analysis/sanitizers.sh` | ASan/UBSan runtime | 5 min |
| `scripts/analysis/valgrind.sh` | Memory checking | 5 min |
| `scripts/fuzz_asan.sh` | Fuzzing with ASan | 2-10 min |

## qa.sh - Main Orchestrator

### Usage

```bash
./scripts/qa.sh [options]

Options:
  --container NAME   Container to use (default: php83-dev)
  --mode MODE        fast|standard|full|security|fuzz
  --skip-build       Skip C extension build
  --php-only         Only run PHP analysis
  --fail-fast        Stop at first error with detailed output
  --help             Show help
```

### Modes

| Mode | Phases | Duration | Use Case |
|------|--------|----------|----------|
| `fast` | 1-4 | ~2 min | Quick static analysis |
| `standard` | 1-5 | ~5 min | **Default, before commits** |
| `full` | 1-6 | ~15 min | Pre-release verification |
| `security` | 1-6 + Gitleaks | ~15 min | Security audit |
| `fuzz` | 1-4 + Fuzzing | ~10 min | Fuzz testing |

### 6-Phase Pipeline

```
┌─────────────────────────────────────────────────────────────┐
│                    qa.sh Pipeline                           │
├─────────────────────────────────────────────────────────────┤
│ Phase 1: Host-side Checks                                   │
│   ├── Gitleaks (security/full modes)                        │
│   ├── PHPStan Level 8                                       │
│   └── PHPCS PSR-12                                          │
├─────────────────────────────────────────────────────────────┤
│ Phase 2: Container Setup                                    │
│   ├── Start Docker container                                │
│   └── Install QA tools (bear, clang-tidy, cppcheck)         │
├─────────────────────────────────────────────────────────────┤
│ Phase 3: Build Extension                                    │
│   └── phpize && configure && bear -- make                   │
├─────────────────────────────────────────────────────────────┤
│ Phase 4: C/C++ Static Analysis                              │
│   ├── clang-tidy (modernize, performance, bugprone)         │
│   └── cppcheck (warnings, performance, portability)         │
├─────────────────────────────────────────────────────────────┤
│ Phase 5: Unit Tests                              [standard+] │
│   └── PHPT test suite via run-tests.php                     │
├─────────────────────────────────────────────────────────────┤
│ Phase 6: Dynamic Analysis                           [full+] │
│   ├── AddressSanitizer + LeakSanitizer                      │
│   ├── UndefinedBehaviorSanitizer                            │
│   ├── Valgrind memcheck                                     │
│   └── Fuzzing (fuzz mode only)                              │
└─────────────────────────────────────────────────────────────┘
```

## test_matrix.sh - Multi-Version Testing

### Usage

```bash
./scripts/test_matrix.sh [container] [firebird_server] [test_files...]

# Examples:
./scripts/test_matrix.sh                              # All containers
./scripts/test_matrix.sh php84-dev                    # Specific container
./scripts/test_matrix.sh php85-fb5-dev firebird50     # With Firebird server
./scripts/test_matrix.sh "" "" tests/fbird_blob_001.phpt  # Specific test
```

### Test Matrix

| Container | PHP | Firebird Client | Target Servers |
|-----------|-----|-----------------|----------------|
| `php81-dev` | 8.1 | 4.x (apt) | FB 3.0, 4.0 |
| `php82-dev` | 8.2 | 4.x (apt) | FB 3.0, 4.0 |
| `php83-dev` | 8.3 | 4.x (apt) | FB 3.0, 4.0 |
| `php84-dev` | 8.4 | 4.x (apt) | FB 3.0, 4.0 |
| `php84-fb3-dev` | 8.4 | 3.0.12 | FB 2.5, 3.0 |
| `php85-dev` | 8.5 | 4.x (apt) | FB 3.0, 4.0 |
| `php85-fb5-dev` | 8.5 | 5.x | FB 4.0, 5.0 |

### Firebird Server Options

```bash
# Override default server
./scripts/test_matrix.sh php84-dev firebird25   # Firebird 2.5
./scripts/test_matrix.sh php84-dev firebird30   # Firebird 3.0
./scripts/test_matrix.sh php84-dev firebird40   # Firebird 4.0
./scripts/test_matrix.sh php84-dev firebird50   # Firebird 5.0
```

## test.sh - PHPT Test Runner

### Usage

```bash
# Run all tests
./scripts/test.sh

# Run specific test
./scripts/test.sh tests/fbird_blob_001.phpt

# Run by name (auto-resolves path)
./scripts/test.sh fbird_blob_001
```

### Environment

- Loads `firebird` extension automatically
- Loads `pcntl` extension if available (for fork tests)
- Uses `TEST_PHP_EXECUTABLE=/usr/local/bin/php`

## Static Analysis Configuration

### clang-tidy (`.clang-tidy`)

**Enabled Check Groups:**
- `modernize-*` - C++17 modernization
- `performance-*` - Performance optimizations
- `readability-*` - Code readability
- `bugprone-*` - Bug-prone patterns
- `cppcoreguidelines-*` - Core Guidelines

**Blocking Errors (WarningsAsErrors):**
```
modernize-use-nullptr
modernize-use-auto
modernize-use-override
performance-move-const-arg
bugprone-use-after-move
```

**Disabled (PHP/Zend API conflicts):**
```
-performance-no-int-to-ptr         # ZEND_BEGIN_ARG_INFO macros
-bugprone-macro-parentheses        # CHECK_LINK macro pattern
-readability-function-cognitive-complexity  # PHP macro expansion
-cppcoreguidelines-narrowing-conversions    # PHP API mixed types
```

### cppcheck (`.cppcheck-suppressions`)

**Standards:** C17, C++17

**Enabled Checks:**
- `warning` - General warnings
- `performance` - Performance issues
- `portability` - Cross-platform issues

**Known Suppressions:**
```
staticFunction:fbird_datetime.c:50   # External linkage functions
staticFunction:fbird_datetime.c:270
missingIncludeSystem                 # System headers resolved at compile
preprocessorErrorDirective:php_fbird_includes.h  # Version check
```

### PHPStan (`phpstan.neon`)

**Level:** 8 (strictest)

**Configuration:**
```yaml
parameters:
    level: 8
    paths:
        - src/
    ignoreErrors:
        - '#^Function fbird_\w+ not found\.$#'  # C extension functions
```

## Dynamic Analysis

### AddressSanitizer (ASan)

**Build Flags:**
```bash
CFLAGS="-fno-omit-frame-pointer -g -O1 -fsanitize=address,leak"
```

**Runtime Options:**
```bash
ASAN_OPTIONS="abort_on_error=1:detect_leaks=1:check_initialization_order=1"
```

**Container:** `php83-asan` (PHP built with ASan)

### UndefinedBehaviorSanitizer (UBSan)

**Build Flags:**
```bash
CFLAGS="-fno-omit-frame-pointer -g -O1 -fsanitize=undefined -fno-sanitize-recover=all"
```

**Runtime Options:**
```bash
UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"
```

### Valgrind

**Best Practices (2024-2025):**

1. `USE_ZEND_ALLOC=0` - Disable Zend's memory manager so Valgrind sees real malloc/free
2. `ZEND_DONT_UNLOAD_MODULES=1` - Keep modules loaded for proper symbol resolution
3. Use suppression file for known PHP/Firebird false positives
4. Look for `definite` and `indirect` leaks (not just `reachable`)
5. Use `--track-origins=yes` for better diagnostics

**Options:**
```bash
valgrind --tool=memcheck \
         --leak-check=full \
         --show-leak-kinds=definite,indirect,possible \
         --track-origins=yes \
         --error-exitcode=1 \
         --errors-for-leak-kinds=definite,indirect \
         --suppressions=valgrind-php.supp
```

**Environment (CRITICAL):**
```bash
export USE_ZEND_ALLOC=0             # Disable Zend allocator for precise tracking
export ZEND_DONT_UNLOAD_MODULES=1   # Keep modules loaded for stack traces
```

## Fuzzing

### Usage

```bash
# Run with ASan container
./scripts/fuzz_asan.sh 1000  # 1000 iterations

# Or via qa.sh
./scripts/qa.sh --mode fuzz
```

### Configuration

- **Container:** `php83-asan`
- **Output:** `fuzz/reports/fuzz_report.sarif`
- **Corpus:** `fuzz/corpus/`

## Tuning Guide

### Adding clang-tidy Checks

Edit `.clang-tidy`:
```yaml
Checks: >
  modernize-*,
  performance-*,
  # Add new check:
  misc-redundant-expression,
```

### Suppressing False Positives

**cppcheck (line-specific):**
```
// cppcheck-suppress unusedFunction
void my_internal_function() { ... }
```

**cppcheck (file-level):**
Add to `.cppcheck-suppressions`:
```
unusedFunction:myfile.c:123
```

**clang-tidy (inline):**
```c
// NOLINTNEXTLINE(bugprone-narrowing-conversions)
int x = (int)long_value;
```

### Adjusting Sanitizer Tests

Edit `scripts/analysis/sanitizers.sh`:
```bash
SAN_TESTS=(
    "tests/sanitizer/asan_basic.php"
    "tests/sanitizer/your_new_test.php"  # Add test
)
```

### Custom Test Selection

```bash
# Run subset with qa.sh (not directly supported, use test_matrix.sh)
./scripts/test_matrix.sh php83-dev "" tests/fbird_blob*.phpt
```

## Troubleshooting

### PHPStan not found
```bash
composer install --dev
```

### Gitleaks not installed
```bash
go install github.com/gitleaks/gitleaks/v8@latest
export PATH=$PATH:~/go/bin
```

### ASan container issues
```bash
# Ensure container is built with ASan-enabled PHP
docker compose up -d php83-asan
docker compose logs php83-asan
```

### Valgrind false positives
Add suppression to `valgrind-php.supp`:
```
{
   php-known-leak
   Memcheck:Leak
   match-leak-kinds: reachable
   fun:malloc
   ...
   fun:php_module_startup
}
```

### Permission errors after root operations
```bash
# The scripts handle this, but manually:
docker compose exec -u root php83-dev chown -R $(id -u):$(id -g) /ext
```

### clang-tidy missing compile_commands.json
```bash
# Rebuild with bear
docker compose exec php83-dev bash -c "cd /ext && make clean && phpize && ./configure && bear -- make"
```

## CI/CD Integration

The QA pipeline maps to GitHub Actions workflows:

| Local Command | CI Workflow |
|---------------|-------------|
| `./scripts/qa.sh --php-only` | `code-quality.yml` → php-analysis |
| `./scripts/qa.sh --mode fast` | `code-quality.yml` (all jobs) |
| `./scripts/qa.sh --mode standard` | `main.yml` → build-test matrix |
| `./scripts/qa.sh --mode full` | `sanitizers.yml` |
| `./scripts/qa.sh --mode security` | `code-quality.yml` + `codeql.yml` |

## See Also

- [SANITIZER_STRATEGY.md](SANITIZER_STRATEGY.md) - Detailed sanitizer configuration
- [STATIC_ANALYSIS_INTEGRATION.md](STATIC_ANALYSIS_INTEGRATION.md) - Analysis tool setup
- [docker.md](docker.md) - Docker environment configuration
