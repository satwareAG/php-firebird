# Local CI Testing

This document describes how to run CI tests locally using Docker Compose.

## Quick Start

```bash
# Run tests for a single PHP/Firebird combo (~4 min)
./scripts/test-local.sh 8.4 3 tests/

# Run the full test matrix (all 12 combos, ~30 min)
./scripts/test_matrix.sh

# Run a single container
./scripts/test_matrix.sh php84-dev          # PHP 8.4, FB 4.0
./scripts/test_matrix.sh php85-fb5-dev       # PHP 8.5, FB 5.0
```

## Test Matrix

| Container | PHP | FB Client | FB Server |
|-----------|-----|-----------|-----------|
| `php82-fb3-dev` | 8.2 | 3.0 | firebird30 |
| `php82-dev` | 8.2 | 4.0 | firebird40 |
| `php82-fb5-dev` | 8.2 | 5.0 | firebird50 |
| `php83-fb3-dev` | 8.3 | 3.0 | firebird30 |
| `php83-dev` | 8.3 | 4.0 | firebird40 |
| `php83-fb5-dev` | 8.3 | 5.0 | firebird50 |
| `php84-fb3-dev` | 8.4 | 3.0 | firebird30 |
| `php84-dev` | 8.4 | 4.0 | firebird40 |
| `php84-fb5-dev` | 8.4 | 5.0 | firebird50 |
| `php85-fb3-dev` | 8.5 | 3.0 | firebird30 |
| `php85-dev` | 8.5 | 4.0 | firebird40 |
| `php85-fb5-dev` | 8.5 | 5.0 | firebird50 |

Sanitizer containers (use explicitly):
```bash
./scripts/test_matrix.sh php83-tsan   # ThreadSanitizer + ZTS
./scripts/test_matrix.sh php83-asan   # AddressSanitizer
```

## test-local.sh vs test_matrix.sh

| Script | Scope | Speed | Use Case |
|--------|-------|-------|----------|
| `test-local.sh` | Single combo, ephemeral containers | ~30s startup | Quick iteration |
| `test_matrix.sh` | All/partial matrix, persistent containers | ~4min per cell | Full CI parity |

## QA Checks

```bash
# Run all QA checks (PHPStan, PHPCS, clang-tidy, cppcheck, gitleaks)
./scripts/qa.sh

# Individual analyzers
./scripts/analysis/sanitizers.sh asan    # AddressSanitizer
./scripts/analysis/sanitizers.sh ubsan   # UndefinedBehaviorSanitizer
./scripts/analysis/valgrind.sh           # Valgrind memcheck
./scripts/analysis/cppcheck.sh           # C/C++ static analysis
./scripts/analysis/clang_tidy.sh         # Clang-Tidy
```

## References

- [act Documentation](https://nektosact.com/) - For running GitHub Actions locally
- [catthehacker Docker Images](https://github.com/catthehacker/docker_images)
- [GitHub Actions Documentation](https://docs.github.com/en/actions)
