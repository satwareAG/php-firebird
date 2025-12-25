# Implementation Plan: Refactor test_with_act.sh for CI/Local Parity

[Overview]
Refactor `scripts/test_with_act.sh` to serve as a CI pre-flight validator that uses local docker-compose containers for actual testing, with `act` relegated to workflow syntax verification only.

The goal is to ensure that running local scripts (`qa.sh`, `test_matrix.sh`) produces results identical to GitHub Actions CI workflows (`code-quality.yml`, `main.yml`). This eliminates the current environment mismatch where `act` runs in isolated containers that cannot access the local Firebird server infrastructure.

**Current Problem:**
- `act` runs workflows in isolation without access to local docker-compose Firebird containers
- Local scripts and CI workflows implement similar checks with different configurations
- No unified "pre-push validation" that guarantees CI will pass

**Solution:**
- Transform `test_with_act.sh` into a unified CI pre-flight script
- Add modes that mirror each CI workflow using local infrastructure
- Use `act --dryrun` only for workflow YAML syntax validation
- Delegate actual testing to existing optimized scripts (`qa.sh`, `test_matrix.sh`)

[Types]
No new types needed - this is a shell script refactoring.

Environment variables used:
- `CI_PREFLIGHT_MODE`: Current operational mode (qa|matrix|full|syntax)
- `CONTAINER`: Target PHP container for testing (default: php83-dev)
- `FIREBIRD_SERVER`: Target Firebird server version (firebird30|firebird40|firebird50)
- `SKIP_BUILD`: Skip C extension compilation if already built
- `PARALLEL`: Run tests in parallel where possible

Exit codes:
- 0: All checks passed
- 1: Quality checks failed
- 2: Build failed
- 3: Tests failed
- 4: Workflow syntax invalid

[Files]
Single file modification with significant refactoring.

**Modified Files:**
- `scripts/test_with_act.sh` - Complete rewrite with new architecture

**No new files needed** - the script will delegate to existing infrastructure:
- `scripts/qa.sh` - Code quality checks (PHPStan, PHPCS, clang-tidy, cppcheck)
- `scripts/test_matrix.sh` - Compatibility testing across PHP/Firebird versions
- `docker/docker-compose.yml` - Local container definitions

[Functions]
Shell functions to implement in the refactored test_with_act.sh.

**New Functions:**

1. `usage()` - Display comprehensive help with examples for each mode

2. `check_prerequisites()` - Verify required tools are available
   - Check: docker, docker compose, composer
   - Optional: act (only for --syntax mode)
   - Optional: gitleaks (for security scans)

3. `run_qa_mode()` - Mirror code-quality.yml workflow
   - Calls `scripts/qa.sh --mode fast` for C analysis
   - Runs PHPStan and PHPCS from host
   - Runs Gitleaks secret scan
   - Maps to CI jobs: php-analysis, c-analysis, secrets-scan

4. `run_matrix_mode()` - Mirror main.yml linux-matrix-build workflow
   - Calls `scripts/test_matrix.sh` with specified containers
   - Supports `--php` and `--fb` flags to select specific matrix cell
   - Default: runs representative subset (php83-dev + firebird40)

5. `run_full_mode()` - Complete CI simulation
   - Combines qa_mode + matrix_mode
   - Runs security scan (Gitleaks)
   - Reports comprehensive summary

6. `run_syntax_mode()` - Workflow YAML validation only
   - Uses `act --dryrun -n` to validate workflow syntax
   - Does NOT execute any actual tests
   - Fast check for YAML errors before push

7. `print_summary()` - Display test results summary
   - Shows pass/fail for each phase
   - Calculates total time
   - Provides actionable next steps on failure

8. `map_ci_matrix_to_local()` - Convert CI matrix values to local containers
   - CI PHP 8.3 → local php83-dev
   - CI Firebird 4.0 → local firebird40
   - Handles special cases (FB 3.0 client → php84-fb3-dev)

**Removed Functions:**
- `run_act_matrix()` - Replaced by delegation to test_matrix.sh
- `run_code_quality()` - Replaced by delegation to qa.sh
- `run_all()` - Replaced by run_full_mode()

[Classes]
No classes - shell script implementation.

[Dependencies]
No new dependencies required.

Existing requirements (checked by check_prerequisites):
- docker
- docker compose (V2+)
- bash 4.0+
- composer (for PHPStan/PHPCS)

Optional (for specific modes):
- act (only for --syntax mode)
- gitleaks (only for security scan in --full mode)

[Testing]
Validation approach for the refactored script.

**Manual Testing Steps:**

1. **QA Mode Test:**
   ```bash
   ./scripts/test_with_act.sh --qa
   # Expected: Runs PHPStan, PHPCS, clang-tidy, cppcheck
   # Should match: code-quality.yml results
   ```

2. **Matrix Mode Test (Single Cell):**
   ```bash
   ./scripts/test_with_act.sh --matrix --php 8.3 --fb 4.0
   # Expected: Runs tests in php83-dev against firebird40
   # Should match: main.yml PHP 8.3 / Firebird 4.0 job
   ```

3. **Matrix Mode Test (Full):**
   ```bash
   ./scripts/test_with_act.sh --matrix --all
   # Expected: Runs all 7 PHP containers × 4 Firebird servers
   # Should match: Complete main.yml matrix
   ```

4. **Full Pre-flight:**
   ```bash
   ./scripts/test_with_act.sh --full
   # Expected: QA + Matrix (representative) + Security
   # Should match: All CI workflows passing
   ```

5. **Syntax Validation:**
   ```bash
   ./scripts/test_with_act.sh --syntax
   # Expected: Validates workflow YAML without running tests
   # Uses: act --dryrun -n
   ```

**Expected Parity:**
- If `./scripts/test_with_act.sh --full` passes locally, CI MUST pass
- If CI passes, `./scripts/test_with_act.sh --full` MUST pass locally

[Implementation Order]
Incremental implementation sequence following Baby Steps™.

1. **Backup and create new structure**
   - Copy current test_with_act.sh to test_with_act.sh.bak
   - Create new file header with usage documentation

2. **Implement check_prerequisites()**
   - Validate docker, docker compose
   - Validate composer for PHP analysis
   - Warn (not fail) if act/gitleaks missing

3. **Implement map_ci_matrix_to_local()**
   - Create PHP version → container name mapping
   - Create Firebird version → server name mapping
   - Handle edge cases (FB3 client, FB5 client)

4. **Implement run_qa_mode()**
   - Call qa.sh with appropriate flags
   - Run Gitleaks if available
   - Capture and report results

5. **Implement run_matrix_mode()**
   - Parse --php and --fb flags
   - Map to local container/server
   - Call test_matrix.sh with parameters
   - Support --all flag for full matrix

6. **Implement run_syntax_mode()**
   - Check if act is installed
   - Run act --dryrun for each workflow
   - Report YAML validation results

7. **Implement run_full_mode()**
   - Orchestrate qa_mode + matrix_mode
   - Add timing information
   - Generate comprehensive report

8. **Implement print_summary()**
   - Collect results from all phases
   - Display pass/fail table
   - Provide actionable recommendations

9. **Implement argument parsing**
   - Support: --qa, --matrix, --full, --syntax
   - Support: --php VER, --fb VER, --all
   - Support: --container NAME, --skip-build
   - Default mode: --qa (fast feedback)

10. **Test and validate**
    - Run each mode and verify output
    - Compare with actual CI results
    - Update documentation

---

## Detailed Function Specifications

### run_qa_mode() Flow

```bash
run_qa_mode() {
    echo "=== CI Quality Check (mirrors code-quality.yml) ==="
    
    # 1. PHP Analysis (mirrors php-analysis job)
    echo ">> PHP Static Analysis (PHPStan + PHPCS)..."
    ./scripts/qa.sh --php-only
    
    # 2. C Analysis (mirrors c-analysis job)
    echo ">> C/C++ Static Analysis (clang-tidy + cppcheck)..."
    ./scripts/qa.sh --container "$CONTAINER" --mode fast --skip-build
    
    # 3. Secret Detection (mirrors secrets-scan job)
    if command -v gitleaks &>/dev/null; then
        echo ">> Secret Detection (Gitleaks)..."
        gitleaks detect --source . --no-git ${GITLEAKS_CONFIG:+--config=$GITLEAKS_CONFIG}
    fi
}
```

### run_matrix_mode() Flow

```bash
run_matrix_mode() {
    echo "=== CI Build Matrix (mirrors main.yml) ==="
    
    if [ "$RUN_ALL_MATRIX" = true ]; then
        # Full matrix (all combinations)
        ./scripts/test_matrix.sh
    else
        # Single matrix cell
        local container=$(map_php_to_container "$PHP_VERSION")
        local server=$(map_fb_to_server "$FB_VERSION")
        ./scripts/test_matrix.sh "$container" "$server"
    fi
}
```

### CI-to-Local Mapping Table

| CI Value | Local Container | Local Server |
|----------|----------------|--------------|
| php-version: 8.1 | php81-dev | - |
| php-version: 8.2 | php82-dev | - |
| php-version: 8.3 | php83-dev | - |
| php-version: 8.4 | php84-dev | - |
| php-version: 8.5 | php85-dev | - |
| firebird-version: 2.5 | php84-fb3-dev* | firebird25 |
| firebird-version: 3.0 | php84-fb3-dev* | firebird30 |
| firebird-version: 4.0 | (any) | firebird40 |
| firebird-version: 5.0 | php85-fb5-dev* | firebird50 |

*Special client library containers for version-specific testing

### Command Examples

```bash
# Quick quality check before commit (default)
./scripts/test_with_act.sh --qa

# Test specific PHP/Firebird combination
./scripts/test_with_act.sh --matrix --php 8.4 --fb 4.0

# Full CI simulation before push
./scripts/test_with_act.sh --full

# Validate workflow YAML syntax only
./scripts/test_with_act.sh --syntax

# Run full matrix (all combinations - slow!)
./scripts/test_with_act.sh --matrix --all

# Use specific container for QA
./scripts/test_with_act.sh --qa --container php85-fb5-dev
```
