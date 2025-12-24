# Local CI Testing with Act

This guide explains how to run GitHub Actions workflows locally using [act](https://github.com/nektos/act) for faster debugging and validation before pushing changes.

## Prerequisites

### Install act

```bash
# Arch/Manjaro/CachyOS
yay -S act

# macOS
brew install act

# Other platforms
curl -s https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash
```

### Verify Installation

```bash
act --version
# Expected: act version 0.2.x
```

## Understanding the Workflow Structure

The `php-firebird` CI uses a 20-job matrix:
- **PHP versions**: 8.1, 8.2, 8.3, 8.4, 8.5
- **Firebird versions**: 2.5, 3.0, 4.0, 5.0

Each job runs in a `php:X.X-cli-bookworm` Docker container with Firebird installed via IBSurgeon scripts.

## Running Tests Locally

### List Available Workflows

```bash
act -l
```

### Run Specific Matrix Combination

For faster debugging, run a single matrix combination:

```bash
# Run PHP 8.3 with Firebird 4.0
act -j linux-matrix-build \
    --matrix php-version:8.3 \
    --matrix firebird-version:4.0
```

### Run Full Matrix (All 20 Combinations)

⚠️ **Warning**: This takes a long time and significant resources.

```bash
act -W .github/workflows/main.yml
```

### Run with Verbose Output

```bash
act -j linux-matrix-build \
    --matrix php-version:8.3 \
    --matrix firebird-version:4.0 \
    --verbose
```

## Configuration for GitHub Actions Parity

### Container Images

Act uses different default images than GitHub Actions. For exact parity:

```bash
# Use the same PHP bookworm images as the workflow
act -P php:8.3-cli-bookworm=php:8.3-cli-bookworm
```

### Environment Variables

Set the same environment variables as the workflow:

```bash
act -j linux-matrix-build \
    --env ISC_USER=SYSDBA \
    --env ISC_PASSWORD=masterkey \
    --env FIREBIRD_HOST=127.0.0.1 \
    --env TEST_DEBUG=1 \
    --matrix php-version:8.3 \
    --matrix firebird-version:4.0
```

### Secrets (if needed)

```bash
# Interactive secrets prompt
act -s GITHUB_TOKEN

# From file
act --secret-file .secrets
```

### `.actrc` Configuration File

Create a `.actrc` file in the project root for persistent settings:

```
-P php:8.1-cli-bookworm=php:8.1-cli-bookworm
-P php:8.2-cli-bookworm=php:8.2-cli-bookworm
-P php:8.3-cli-bookworm=php:8.3-cli-bookworm
-P php:8.4-cli-bookworm=php:8.4-cli-bookworm
-P php:8.5-cli-bookworm=php:8.5-cli-bookworm
--container-architecture linux/amd64
```

## Limitations and Workarounds

### 1. Service Containers

Act doesn't fully support `services:` like GitHub Actions. Our workflow uses in-container Firebird installation which works with act.

### 2. Network Differences

The workflow connects to Firebird on `127.0.0.1:3050` within the container. This works identically in act.

### 3. Architecture (Apple Silicon)

On M1/M2 Macs, force x86_64 architecture:

```bash
act --container-architecture linux/amd64
```

### 4. Caching

Act doesn't share Docker layer cache with GitHub Actions. First runs take longer.

## Debugging Tips

### Interactive Shell

Run a shell in the container for debugging:

```bash
act -j linux-matrix-build \
    --matrix php-version:8.3 \
    --matrix firebird-version:4.0 \
    --reuse \
    --step "Build PHP extension"
```

### Skip Steps

Run only specific steps:

```bash
# Run up to and including "Build PHP extension" step
act -j linux-matrix-build \
    --matrix php-version:8.3 \
    --matrix firebird-version:4.0 \
    --step "build"
```

### Dry Run

See what would be executed without running:

```bash
act -n
```

## Common Issues

### Issue: Extension Not Loaded in Tests

**Symptom**: `php -m` shows "Extension not listed" during PHPT tests.

**Solution**: The workflow uses `PHP_TEST_SHARED_EXTENSIONS` environment variable to pass `-d extension=...` to run-tests.php. This is the correct variable - NOT `TEST_PHP_ARGS`.

### Issue: Firebird Connection Refused

**Symptom**: Tests fail with "connection refused" errors.

**Solution**: Ensure Firebird server started successfully. Check:
```bash
nc -z 127.0.0.1 3050
```

### Issue: Permission Denied on /opt/firebird/data

**Symptom**: Tests fail creating databases.

**Solution**: The workflow creates `/opt/firebird/data` with `chmod 777`. Verify this step completed.

## Quick Reference

```bash
# Most common command for local testing
act -j linux-matrix-build \
    --matrix php-version:8.3 \
    --matrix firebird-version:4.0 \
    -v

# Check GitHub run status for comparison
gh run list --branch $(git branch --show-current) --limit 5
gh run view <run-id> --log
```

## See Also

- [nektos/act Documentation](https://github.com/nektos/act)
- [GitHub Actions Workflow Syntax](https://docs.github.com/en/actions/reference/workflow-syntax-for-github-actions)
- [php-firebird CI Workflow](.github/workflows/main.yml)
