# Local CI Testing with act

This document describes how to run GitHub Actions workflows locally using `act` and the hybrid testing strategy for the php-firebird project.

## Overview

The php-firebird project uses a **hybrid testing strategy** because some workflows use GitHub Actions service containers (Firebird database), which have limited support in `act` v0.2.83.

### Testing Strategy Decision Matrix

| Workflow | Service Containers | Best Local Method | Command |
|----------|-------------------|-------------------|---------|
| `code-quality.yml` | No | **act** (direct) | `./scripts/test_with_act.sh act code-quality` |
| `main.yml` | Yes (Firebird) | **Docker Compose** | `./scripts/test_with_act.sh --matrix` |
| `coverage.yml` | Yes (Firebird) | **Docker Compose** | `./scripts/test_with_act.sh --coverage` |
| `sanitizers.yml` | Yes (Firebird) | **Docker Compose** | `./scripts/test_with_act.sh --sanitizers` |
| `codeql.yml` | No | **act** (limited) | `act -W .github/workflows/codeql.yml` |

## Prerequisites

### Required Tools

```bash
# act - GitHub Actions local runner
sudo pacman -S act              # Arch Linux
brew install act                # macOS
# See: https://github.com/nektos/act#installation

# Docker (required)
docker --version                # Must be installed

# Optional: docker-compose for hybrid testing
docker compose version          # V2+ required
```

### Configuration

The project includes an `.actrc` file that configures act for optimal CI parity:

```bash
# View current configuration
cat .actrc

# Key settings:
# - Uses catthehacker/ubuntu:act-latest images (medium size, good compatibility)
# - x86_64 architecture for consistent builds
# - Artifact server path for upload/download-artifact support
```

## Quick Start

### Full CI Simulation (Recommended)

```bash
# Run complete CI pre-flight check (matches GitHub Actions behavior)
./scripts/test_with_act.sh --full

# This runs:
# 1. QA checks (PHPStan, PHPCS, clang-tidy, cppcheck, gitleaks)
# 2. Matrix tests (PHP × Firebird combinations via Docker Compose)
```

### Individual Workflows

```bash
# Code Quality (works with act directly)
./scripts/test_with_act.sh act code-quality

# Matrix Build (uses Docker Compose due to service containers)
./scripts/test_with_act.sh --matrix --php 8.4 --fb 5.0

# Coverage (uses Docker Compose)
./scripts/test_with_act.sh --coverage

# Sanitizers (uses Docker Compose)
./scripts/test_with_act.sh --sanitizers
```

## act Usage Details

### Basic Commands

```bash
# List all workflows and jobs
act -l

# Dry-run a workflow (validate syntax)
act -W .github/workflows/code-quality.yml -n

# Run a specific workflow
act -W .github/workflows/code-quality.yml

# Run a specific job
act -j php-analysis -W .github/workflows/code-quality.yml

# Run with verbose output
act -v -W .github/workflows/code-quality.yml
```

### Environment Variables and Secrets

```bash
# Using environment variables
act -e event.json --env-file .act.env

# Using secrets
act -s MY_SECRET=value
act --secret-file .act.secrets

# Using GitHub token (for API calls)
act -s GITHUB_TOKEN="$(gh auth token)"
```

### Runner Image Selection

| Image Size | Image Name | Size | Use Case |
|------------|------------|------|----------|
| **Micro** | `node:16-buster-slim` | ~200MB | Fast, minimal |
| **Medium** | `catthehacker/ubuntu:act-latest` | ~2GB | Default, good balance |
| **Large** | `catthehacker/ubuntu:full-latest` | ~18GB | Full GitHub parity |

```bash
# Override runner image for a single run
act -P ubuntu-latest=catthehacker/ubuntu:full-latest

# The .actrc file sets medium as default
```

## Known Limitations

### Service Container Bug (act v0.2.83)

**Issue**: Workflows with `services:` blocks cause a panic in act v0.2.83:
```
panic: runtime error: invalid memory address or nil pointer dereference
[signal SIGSEGV: segmentation violation code=0x1 addr=0xc8 pc=...]
```

**Affected Workflows**:
- `main.yml` (Firebird service container)
- `coverage.yml` (Firebird service container)
- `sanitizers.yml` (Firebird service container)

**Workaround**: Use Docker Compose via `test_with_act.sh --matrix`, `--coverage`, or `--sanitizers` modes.

### Other act Limitations

| Feature | GitHub Actions | act | Workaround |
|---------|----------------|-----|------------|
| Service containers | ✅ Full support | ⚠️ Buggy | Use Docker Compose |
| Caching (actions/cache) | ✅ Full support | ⚠️ Limited | Manual caching |
| Artifacts | ⚠️ Limited | ✅ `--artifact-server-path` | Set in .actrc |
| Matrix expansion | ✅ Full support | ✅ Supported | - |
| Container jobs | ✅ Full support | ✅ Supported | - |
| Secrets | ✅ Encrypted | ✅ Via flags/files | Use --secret-file |
| GITHUB_TOKEN | ✅ Auto-provided | ❌ Manual | Use `gh auth token` |

## Hybrid Testing Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    test_with_act.sh                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────┐    ┌────────────────────────────────────┐  │
│  │  act mode       │    │  Docker Compose modes              │  │
│  │                 │    │                                     │  │
│  │  • code-quality │    │  • --qa       (qa.sh)              │  │
│  │  • codeql       │    │  • --matrix   (test_matrix.sh)     │  │
│  │                 │    │  • --coverage (coverage.sh)        │  │
│  │  Direct workflow│    │  • --sanitizers                    │  │
│  │  execution      │    │                                     │  │
│  └─────────────────┘    │  Uses existing Docker containers   │  │
│         │               │  from docker/docker-compose.yml    │  │
│         ▼               └────────────────────────────────────┘  │
│  ┌─────────────────┐              │                             │
│  │ act runner      │              ▼                             │
│  │ (catthehacker)  │    ┌────────────────────────────────────┐  │
│  └─────────────────┘    │ Local Firebird containers          │  │
│                         │ • firebird25, firebird30           │  │
│                         │ • firebird40, firebird50           │  │
│                         │ • php81-dev ... php85-dev          │  │
│                         └────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

## CI Parity Checklist

Before pushing, ensure all checks pass locally:

```bash
# ✅ Quick validation (QA only, ~2 min)
./scripts/test_with_act.sh --qa

# ✅ Standard validation (QA + single matrix cell, ~5 min)
./scripts/test_with_act.sh --full

# ✅ Full validation (QA + all matrix combinations, ~20 min)
./scripts/test_with_act.sh --full --all
```

### What Each Mode Tests

| Mode | PHPStan | PHPCS | clang-tidy | cppcheck | Gitleaks | PHPT Tests | Coverage |
|------|---------|-------|------------|----------|----------|------------|----------|
| `--qa` | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| `--matrix` | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ | ❌ |
| `--coverage` | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ |
| `--full` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |

## Troubleshooting

### act crashes with panic

**Symptom**: `panic: runtime error: invalid memory address or nil pointer dereference`

**Cause**: Service containers not fully supported in act v0.2.83

**Solution**: Use Docker Compose modes instead:
```bash
# Instead of: act -W .github/workflows/main.yml
# Use:
./scripts/test_with_act.sh --matrix
```

### Workflow syntax validation

```bash
# Validate all workflow files
act -l

# Dry-run specific workflow
act -W .github/workflows/code-quality.yml -n

# Check for YAML errors with verbose output
act -W .github/workflows/main.yml -n -v 2>&1 | head -50
```

### Image not found

```bash
# Pull required images manually
docker pull catthehacker/ubuntu:act-latest
docker pull php:8.4-cli-bookworm
docker pull firebirdsql/firebird:5

# Use --pull=false to skip pulling if images exist
act --pull=false -W .github/workflows/code-quality.yml
```

### Slow initial runs

First run downloads actions from GitHub. Enable offline mode after first successful run:

```bash
# First run (downloads actions)
act -W .github/workflows/code-quality.yml

# Subsequent runs (use cached actions)
# Edit .actrc and uncomment: --action-offline-mode
```

## References

- [act Documentation](https://nektosact.com/)
- [act GitHub Repository](https://github.com/nektos/act)
- [catthehacker Docker Images](https://github.com/catthehacker/docker_images)
- [GitHub Actions Documentation](https://docs.github.com/en/actions)