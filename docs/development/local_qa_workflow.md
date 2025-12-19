# Local QA Workflow & Optimization

## Overview

The php-firebird project provides comprehensive local quality assurance tools:

| Script | Purpose | Usage |
|--------|---------|-------|
| `scripts/host/qa_local.sh` | Basic QA (original) | Quick checks during development |
| `scripts/host/qa_full.sh` | **Comprehensive QA** | Full quality checks before committing |
| `scripts/host/pre-commit-hook.sh` | Git pre-commit | Automatic checks on `git commit` |

## Quick Start

```bash
# Install pre-commit hook (recommended)
ln -sf ../../scripts/host/pre-commit-hook.sh .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit

# Install PHP dependencies for PHPStan
composer install --dev

# Run full QA before committing
./scripts/host/qa_full.sh --mode standard
```

## Prerequisites

- Docker & Docker Compose
- PHP + Composer (for PHPStan - optional, can run in container)
- Gitleaks (for secret detection - optional but recommended)

### Installing Optional Tools

```bash
# Gitleaks (Go required)
go install github.com/gitleaks/gitleaks/v8@latest

# PHPStan via Composer
composer install --dev

# Or install cppcheck on host for faster pre-commit
sudo apt install cppcheck    # Debian/Ubuntu
brew install cppcheck        # macOS
```

## qa_full.sh - Comprehensive QA Script

### Usage

```bash
./scripts/host/qa_full.sh [options]

Options:
  --container NAME   Container to use (default: php83-dev)
  --mode MODE        fast|standard|full|security (default: standard)
  --skip-build       Skip C extension build (use existing)
  --php-only         Only run PHP analysis (PHPStan, PHPCS)
  --help             Show help message
```

### Modes

| Mode | Tools Run | Duration | Use Case |
|------|-----------|----------|----------|
| `fast` | PHPStan, clang-tidy, cppcheck | ~2 min | Quick iteration |
| `standard` | fast + unit tests | ~5 min | **Recommended for commits** |
| `full` | standard + ASan + UBSan + Valgrind | ~15 min | Release verification |
| `security` | full + Gitleaks | ~15 min | Security audit |

### Examples

```bash
# Standard checks (recommended before commit)
./scripts/host/qa_full.sh

# Quick static analysis only
./scripts/host/qa_full.sh --mode fast

# Full security audit
./scripts/host/qa_full.sh --mode security

# PHP only (no Docker needed)
./scripts/host/qa_full.sh --php-only

# Target specific PHP version
./scripts/host/qa_full.sh --container php84-dev --mode standard
```

## Tool Coverage

### Static Analysis (Phase 1 + 4)

| Tool | Language | Purpose | Configuration |
|------|----------|---------|---------------|
| **PHPStan** | PHP | Type-safe static analysis | `phpstan.neon` (Level 8) |
| **PHPCS** | PHP | PSR-12 code style | `phpcs.xml` (PSR-12 base) |
| **clang-tidy** | C/C++ | Modern C++ checks | `.clang-tidy` |
| **cppcheck** | C/C++ | Bug detection | `.cppcheck` |
| **Gitleaks** | All | Secret detection | Built-in rules |

### Dynamic Analysis (Phase 6 - Full Mode)

| Tool | Purpose | Slowdown | Catches |
|------|---------|----------|---------|
| **AddressSanitizer** | Memory errors | ~2x | Buffer overflows, use-after-free |
| **LeakSanitizer** | Memory leaks | ~2x | Memory not freed |
| **UndefinedBehaviorSanitizer** | UB detection | ~1.5x | Integer overflow, null deref |
| **Valgrind** | Deep memory check | ~20x | All memory issues |

## Pre-Commit Hook

The pre-commit hook runs lightweight checks on every commit:

1. **Gitleaks** - Block commits with secrets
2. **PHPStan** - Check modified PHP files
3. **cppcheck** - Quick check modified C files
4. **Debug artifacts** - Warn about var_dump, etc.

### Installation

```bash
# Symlink method (updates with repo)
ln -sf ../../scripts/host/pre-commit-hook.sh .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit

# Or copy method
cp scripts/host/pre-commit-hook.sh .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit
```

### Bypass (Emergency Only)

```bash
git commit --no-verify -m "Emergency fix"
```

## Workflow Phases

### Phase 1: Host-side Checks
- Gitleaks secret detection (security/full modes)
- PHPStan PHP analysis
- PHPCS code style

### Phase 2: Container Setup
- Start Docker container
- Install QA tools if missing

### Phase 3: Build Extension
- Clean build with `bear`
- Generate `compile_commands.json`

### Phase 4: Static Analysis
- clang-tidy C++ checks
- cppcheck bug detection

### Phase 5: Unit Tests
- PHP run-tests.php suite

### Phase 6: Dynamic Analysis (full mode)
- AddressSanitizer + LeakSanitizer
- UndefinedBehaviorSanitizer
- Valgrind (optional, slow)

## Container Analysis Scripts

Individual analysis scripts for container use:

```bash
# Run in container context
docker compose exec php83-dev /ext/scripts/container/analysis/sanitizers.sh all
docker compose exec php83-dev /ext/scripts/container/analysis/clang_tidy.sh
docker compose exec php83-dev /ext/scripts/container/analysis/cppcheck.sh
docker compose exec php83-dev /ext/scripts/container/analysis/valgrind.sh
```

## PHPStan Configuration

The `phpstan.neon` file configures Level 8 (strictest) analysis:

```yaml
parameters:
    level: 8
    paths:
        - src/
    ignoreErrors:
        # Allow fbird_* functions from C extension
        - '#^Function fbird_\w+ not found\.$#'
```

## Recommended Workflow

1. **Development**: Code changes
2. **Pre-commit**: Automatic checks via hook
3. **Before PR**: `./scripts/host/qa_full.sh --mode standard`
4. **Before Release**: `./scripts/host/qa_full.sh --mode security`

## Troubleshooting

### PHPStan not found
```bash
composer install --dev
```

### Gitleaks not installed
```bash
go install github.com/gitleaks/gitleaks/v8@latest
# Add ~/go/bin to PATH
```

### Container tools missing
The script auto-installs tools, but manually:
```bash
docker compose exec -u root php83-dev apt-get update
docker compose exec -u root php83-dev apt-get install -y bear clang-tidy cppcheck
```

### Sanitizer false positives
Edit `scripts/container/analysis/lsan.supp` for LeakSanitizer suppressions.

## Legacy Script

The original `qa_local.sh` is still available for simpler checks:

```bash
./scripts/host/qa_local.sh [container_name] [mode]
# mode: 'fast' (default) or 'full'
```

## GitHub Actions CI/CD

The project includes comprehensive CI/CD workflows that complement local QA:

### Workflow Overview

| Workflow | File | Triggers | Purpose |
|----------|------|----------|---------|
| **Main Build** | `main.yml` | Push, PR, Daily | PHP 8.1-8.5 × Firebird 2.5-5.0 matrix |
| **Code Coverage** | `coverage.yml` | Push, PR, Daily | lcov coverage (≥55% gate) |
| **Code Quality** | `code-quality.yml` | Push, PR | PHPStan, PHPCS, cppcheck, Gitleaks |
| **CodeQL** | `codeql.yml` | Push, PR, Weekly | Security scanning (C++ & PHP) |
| **Sanitizers** | `sanitizers.yml` | Push, PR | ASan, UBSan, LSan testing |

### Code Quality Workflow

```yaml
# Three parallel jobs:
php-analysis:    # PHPStan Level 8 + PHPCS PSR-12
c-analysis:      # cppcheck (blocking) + clang-tidy (advisory)
secrets-scan:    # Gitleaks secret detection
```

### CodeQL Security Scanning

- **C/C++ Analysis**: Builds extension with Firebird, runs CodeQL security queries
- **PHP Analysis**: Scans `src/` wrapper classes for vulnerabilities
- **Schedule**: Weekly on Sundays + every PR to main

### Sanitizer CI Jobs

```yaml
asan-ubsan:  # AddressSanitizer + UndefinedBehaviorSanitizer
             # Detects: buffer overflows, use-after-free, undefined behavior
             # BLOCKING: Fails build on errors

lsan:        # LeakSanitizer (standalone)
             # Detects: Memory leaks
             # ADVISORY: Warnings only (false positives possible)
```

### Local vs CI Equivalence

| Local Command | CI Equivalent |
|---------------|---------------|
| `./scripts/host/qa_full.sh --php-only` | Code Quality → php-analysis |
| `./scripts/host/qa_full.sh --mode fast` | Code Quality (all jobs) |
| `./scripts/host/qa_full.sh --mode full` | Sanitizers workflow |
| `./scripts/host/qa_full.sh --mode security` | Code Quality + CodeQL |

### Running Workflows Manually

```bash
# Trigger sanitizers workflow manually (debugging)
gh workflow run sanitizers.yml

# View workflow status
gh run list --workflow=code-quality.yml
```

## See Also

- [STATIC_ANALYSIS_INTEGRATION.md](STATIC_ANALYSIS_INTEGRATION.md) - Detailed tool configuration
- [docker.md](docker.md) - Docker environment setup
- [CODE_COVERAGE_PLAN.md](CODE_COVERAGE_PLAN.md) - Coverage strategy
