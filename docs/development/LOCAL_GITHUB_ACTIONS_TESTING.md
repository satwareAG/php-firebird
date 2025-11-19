# Local GitHub Actions Testing with `act`

## Overview

Complete guide for running and debugging GitHub Actions workflows locally using `act`. This allows testing workflows before pushing to GitHub, debugging issues, and validating changes.

## Quick Commands

### Basic Workflow Execution

```bash
# List all workflows and jobs
act --list

# Run default event (push)
act

# Run specific job
act --job linux-comprehensive-build

# Run with specific matrix
act --job linux-comprehensive-build --matrix php-version:8.3 --matrix build-type:release

# Dry run (validation only)
act --dryrun

# Watch for file changes and re-run
act --watch
```

### Currently Running Workflow

✅ **WORKFLOW IS RUNNING**: PHP 8.3 + Firebird 4.0 + Release build

**Container ID**: `c8653a992195`
**Status**: Installing PHP 8.3 development dependencies
**Matrix**: `php-version:8.3`, `build-type:release`, `firebird-version:4.0`

### Debugging Running Workflows

```bash
# Check running containers
docker ps | grep act

# Inspect container without TTY
docker exec c8653a992195 ps aux | head -10
docker exec c8653a992195 ls -la /home/mw/Projects/php-firebird

# Check PHP installation in container
docker exec c8653a992195 which php8.3
docker exec c8653a992195 php8.3 --version

# Check build progress
docker exec c8653a992195 ls -la modules/ 2>/dev/null || echo "modules/ not created yet"

# Follow logs (if workflow is still running)
docker logs -f c8653a992195
```

## Advanced Usage

### Matrix Testing

```bash
# Test specific PHP version
act --matrix php-version:8.3
act --matrix php-version:8.4
act --matrix php-version:8.5

# Test specific build types
act --matrix build-type:debug
act --matrix build-type:release

# Test specific Firebird versions
act --matrix firebird-version:3.0
act --matrix firebird-version:4.0

# Combine multiple matrix values
act --job linux-comprehensive-build \
  --matrix php-version:8.4 \
  --matrix build-type:debug \
  --matrix firebird-version:3.0
```

### Platform-Specific Jobs

```bash
# Linux job (works locally)
act --job linux-comprehensive-build

# C++ modernization validation
act --job cpp17-modernization-validation

# Cross-platform summary (requires other jobs to complete)
act --job cross-platform-summary

# Windows/macOS jobs (limited local support)
act --job windows-build    # May not work due to Windows requirements
act --job macos-build      # May not work due to macOS requirements
```

### Custom Configuration

```bash
# Use custom Docker image
act --platform ubuntu-latest=ubuntu:22.04

# Set environment variables
act --env DATABASE_URL=sqlite:///test.db
act --env PHP_VERSION=8.3

# Use secrets file
echo "API_KEY=test" > .secrets
act --secret-file .secrets

# Bind working directory (faster)
act --bind

# Verbose debugging
act --verbose --job linux-comprehensive-build
```

## Configuration Files

### `.env` file (optional)
```bash
# .env - Environment variables for local testing
DATABASE_URL=sqlite:///test.db
PHP_VERSION=8.3
FIREBIRD_VERSION=4.0
BUILD_TYPE=release
```

### `.secrets` file (optional)
```bash
# .secrets - Secrets for local testing (DO NOT commit)
API_KEY=local-test-key
DATABASE_PASSWORD=test-password
```

### `.actrc` file (optional)
```bash
# .actrc - Default act configuration
--platform ubuntu-latest=catthehacker/ubuntu:act-latest
--artifact-server-path /tmp/artifacts
--cache-server-path /tmp/cache
--bind
```

## Debugging Techniques

### 1. Step-by-Step Debugging

```bash
# Run with verbose output
act --job linux-comprehensive-build --verbose

# Stop at specific step (manually interrupt)
act --job linux-comprehensive-build
# Ctrl+C when you want to inspect state

# Reuse containers for debugging
act --reuse --job linux-comprehensive-build
```

### 2. Container Inspection

```bash
# Get container ID
CONTAINER_ID=$(docker ps --format "{{.ID}}" | head -1)

# Inspect filesystem
docker exec $CONTAINER_ID ls -la
docker exec $CONTAINER_ID find /tmp -name "*.so" 2>/dev/null
docker exec $CONTAINER_ID df -h

# Check installed packages
docker exec $CONTAINER_ID dpkg -l | grep php
docker exec $CONTAINER_ID dpkg -l | grep firebird

# Verify build environment
docker exec $CONTAINER_ID env | grep PHP
docker exec $CONTAINER_ID which phpize8.3
docker exec $CONTAINER_ID php-config8.3 --version
```

### 3. Build Artifacts

```bash
# Check if extension was built
docker exec $CONTAINER_ID ls -la modules/
docker exec $CONTAINER_ID file modules/interbase.so

# Test extension loading
docker exec $CONTAINER_ID php8.3 -d extension=modules/interbase.so -m | grep interbase

# Check extension functions
docker exec $CONTAINER_ID php8.3 -d extension=modules/interbase.so -r "print_r(get_extension_funcs('interbase'));"
```

### 4. Error Diagnosis

```bash
# Check for compilation errors
docker exec $CONTAINER_ID find . -name "*.log" -exec cat {} \;
docker exec $CONTAINER_ID cat config.log

# Check for missing dependencies
docker exec $CONTAINER_ID ldd modules/interbase.so

# Verify PHP API version
docker exec $CONTAINER_ID php-config8.3 --phpapi
```

## Workflow Limitations in Local Testing

### What Works ✅

- ✅ Linux jobs (`linux-comprehensive-build`, `cpp17-modernization-validation`)
- ✅ Ubuntu-based workflows
- ✅ PHP extension compilation
- ✅ Firebird database setup
- ✅ Static analysis tools (clang-tidy, cppcheck)
- ✅ Matrix testing (different PHP/Firebird versions)
- ✅ Build artifact verification
- ✅ Test execution

### What Has Limited Support ⚠️

- ⚠️ Windows jobs (`windows-build`) - Requires Windows containers
- ⚠️ macOS jobs (`macos-build`) - Requires macOS environment 
- ⚠️ Cross-platform summary - Needs all jobs to complete first
- ⚠️ GitHub-specific actions - Some may not work offline

### Alternatives for Unsupported Jobs

```bash
# For Windows testing (alternative)
# Use Windows Docker images (experimental)
act --platform windows-latest=mcr.microsoft.com/windows/servercore:ltsc2022

# For macOS testing (alternative)  
# Use virtualization or GitHub Codespaces
# Local macOS: Run natively with same commands from workflow

# For cross-platform summary
# Run individual jobs first, then summary
act --job linux-comprehensive-build
act --job cpp17-modernization-validation
act --job cross-platform-summary
```

## Common Issues and Solutions

### 1. Container Permission Issues

```bash
# Problem: Permission denied errors
# Solution: Use --privileged flag
act --privileged --job linux-comprehensive-build

# Or use --userns for user namespace
act --userns host --job linux-comprehensive-build
```

### 2. Network Connectivity

```bash
# Problem: Cannot download packages
# Solution: Check network mode
act --network bridge --job linux-comprehensive-build

# Or use host network (default)
act --network host --job linux-comprehensive-build
```

### 3. Disk Space Issues

```bash
# Problem: Out of disk space
# Solution: Clean up act cache
rm -rf ~/.cache/act/*

# Clean up Docker volumes
docker volume prune
docker system prune -a
```

### 4. GitHub Actions Not Found

```bash
# Problem: actions/checkout@v4 not found
# Solution: Enable internet access for action downloads
act --pull --job linux-comprehensive-build

# Force rebuild actions
act --rebuild --job linux-comprehensive-build
```

## Performance Optimization

### 1. Faster Execution

```bash
# Use bind mounts (faster than copy)
act --bind

# Disable pulling existing images
act --pull=false

# Reuse containers between runs
act --reuse

# Use local action cache
act --use-new-action-cache
```

### 2. Reduced Resource Usage

```bash
# Limit concurrent jobs
act --concurrent-jobs 2

# Use smaller Ubuntu image
act --platform ubuntu-latest=ubuntu:22.04

# Skip checkout for repeated runs
act --no-skip-checkout=false
```

## Integration with Development Workflow

### 1. Pre-Push Testing

```bash
#!/bin/bash
# scripts/test-ci.sh
set -e

echo "🧪 Testing GitHub Actions locally..."

# Test Linux build
echo "Testing Linux compilation..."
act --job linux-comprehensive-build --matrix php-version:8.3

# Test C++ modernization
echo "Testing C++17 modernization..."  
act --job cpp17-modernization-validation

echo "✅ All local tests passed!"
```

### 2. Debugging Failed CI

```bash
# When GitHub Actions fails, reproduce locally:

# 1. Check the exact matrix that failed
act --job linux-comprehensive-build \
  --matrix php-version:8.4 \
  --matrix build-type:debug \
  --matrix firebird-version:3.0

# 2. Enable verbose logging
act --verbose --job linux-comprehensive-build

# 3. Keep container running for inspection
act --reuse --job linux-comprehensive-build
```

### 3. Iterative Development

```bash
# Watch for changes and re-run
act --watch --job linux-comprehensive-build

# Test individual steps by modifying workflow temporarily
# (Remove steps after the one you want to debug)
```

## Complete Testing Workflow

### 1. Test All Linux Jobs

```bash
#!/bin/bash
# Complete local testing script

echo "Testing all GitHub Actions jobs locally..."

# Test matrix combinations
for php_version in 8.3 8.4 8.5; do
  for build_type in release debug; do
    for firebird_version in 3.0 4.0; do
      echo "Testing PHP $php_version ($build_type) with Firebird $firebird_version"
      
      act --job linux-comprehensive-build \
        --matrix php-version:$php_version \
        --matrix build-type:$build_type \
        --matrix firebird-version:$firebird_version \
        --quiet
      
      if [ $? -eq 0 ]; then
        echo "✅ PASS: PHP $php_version ($build_type) + Firebird $firebird_version"
      else
        echo "❌ FAIL: PHP $php_version ($build_type) + Firebird $firebird_version"
      fi
    done
  done
done

# Test C++ modernization
echo "Testing C++17 modernization validation..."
act --job cpp17-modernization-validation

echo "🎉 Local testing complete!"
```

### 2. Quick Validation

```bash
# Quick syntax validation
act --dryrun

# Test single matrix combination  
act --job linux-comprehensive-build \
  --matrix php-version:8.3 \
  --matrix build-type:release \
  --matrix firebird-version:4.0

# Verify extension builds
docker exec $(docker ps --format "{{.ID}}" | head -1) \
  bash -c "ls -la modules/ && file modules/*.so"
```

## Summary

### ✅ **Working Now**

The workflow is successfully running locally! Here's what you can do:

**Current Status**: 
- Container running with ID: `c8653a992195`
- Installing PHP 8.3 development environment
- Will build PHP Firebird extension 
- Testing matrix: PHP 8.3 + Firebird 4.0 + Release build

**Available Commands**:
```bash
# Monitor progress
docker logs -f c8653a992195

# Check when complete
docker exec c8653a992195 ls -la modules/

# Run other matrix combinations
act --job linux-comprehensive-build --matrix php-version:8.4
```

**Benefits**: 
- Test workflows before pushing to GitHub
- Debug compilation issues locally  
- Validate matrix combinations
- Faster iteration than GitHub Actions runners
