# Implementation Plan: Enhanced Test Matrix

[Overview]
Create an enhanced test_matrix.sh script that runs PHP/Firebird version combinations in a matrix to verify that all skipped tests pass with the correct configurations.

The current test_matrix.sh supports running tests across PHP containers with optional Firebird server targeting, but lacks:
1. Predefined matrix configurations for specific version combinations
2. Automatic discovery/verification of skipped tests
3. Reporting which skipped tests would pass with alternative configurations

**Current Skipped Tests (3 total):**
| Test | Requirement | Working Configuration |
|------|-------------|----------------------|
| `fb40fields_002.phpt` | FB4+ server, FB<4 client | php84-fb3-dev + firebird40 |
| `fbird_field_info_005.phpt` | FB4+ server, FB<4 client | php84-fb3-dev + firebird40 |
| `long_names_002.phpt` | FB≤3.0 server | any container + firebird30 |

[Types]
No new types needed - this is a shell script enhancement.

Environment variables used:
- `FIREBIRD_HOST`: Override target Firebird server (firebird25|firebird30|firebird40|firebird50)
- `FIREBIRD_DB_DIR`: Override database directory (defaults to /tmp when FIREBIRD_HOST overridden)
- `SKIP_VALIDATION`: Enable mode to find tests that would pass with different configurations

[Files]
Single file modification.

**Modified Files:**
- `scripts/test_matrix.sh` - Add matrix mode, skip validation, enhanced reporting

**No new files needed** - consolidating functionality into existing script.

[Functions]
Shell functions to add/modify in test_matrix.sh.

**New Functions:**
1. `run_matrix_mode()` - Run predefined PHP×Firebird combinations
2. `validate_skipped_tests()` - Find tests that would pass with alternative configs
3. `print_matrix_table()` - Display results in tabular format
4. `get_skip_config()` - Map test files to their required configurations

**Modified Logic:**
- Add `--matrix` flag for full matrix execution
- Add `--validate-skips` flag for skip test validation
- Add `--list-combinations` flag to show possible configs
- Improve summary output with tabular results

[Classes]
No classes - shell script implementation.

[Dependencies]
No new dependencies.

Existing requirements:
- docker
- docker compose
- bash 4.0+ (for associative arrays)

[Testing]
Validation approach.

**Manual Testing Steps:**
1. Run `./scripts/test_matrix.sh --matrix` to execute full matrix
2. Run `./scripts/test_matrix.sh --validate-skips` to verify skip configurations
3. Verify all 3 skipped tests pass with correct configurations:
   - `./scripts/test_matrix.sh php84-fb3-dev firebird40 tests/fb40fields_002.phpt`
   - `./scripts/test_matrix.sh php84-fb3-dev firebird40 tests/fbird_field_info_005.phpt`
   - `./scripts/test_matrix.sh php83-dev firebird30 tests/long_names_002.phpt`

**Expected Results:**
- Matrix mode: All containers × relevant servers pass
- Skip validation: Shows 3 tests that need specific configurations
- Individual tests: 3/3 previously-skipped tests now PASS

[Implementation Order]
Incremental implementation sequence.

1. **Add predefined matrix configurations**
   - Define MATRIX_COMBINATIONS associative array
   - Map containers to compatible servers

2. **Add --matrix flag handling**
   - Parse new flag in argument handling
   - Implement run_matrix_mode() function

3. **Add --validate-skips flag**
   - Implement validate_skipped_tests() function
   - Parse SKIPIF sections to determine requirements

4. **Add --list-combinations flag**
   - Show available PHP containers
   - Show available Firebird servers  
   - Show recommended test configurations

5. **Enhance summary output**
   - Tabular results for matrix runs
   - Per-test pass/skip/fail breakdown
   - Configuration recommendations for skipped tests

6. **Test and verify**
   - Run matrix mode
   - Verify all 3 skipped tests pass with correct configs
   - Update documentation

---

## Detailed Implementation

### Matrix Configurations

```bash
# PHP Container → Default Firebird Client Version
# php81-dev through php85-dev: FB 4.x client (apt firebird-dev)
# php84-fb3-dev: FB 3.x client
# php85-fb5-dev: FB 5.x client

# Firebird Servers Available
# firebird25: 2.5.x (legacy)
# firebird30: 3.0.x  
# firebird40: 4.0.x (default)
# firebird50: 5.0.x (latest)
```

### Skip Test Mapping

```bash
declare -A SKIP_TEST_CONFIG=(
    ["tests/fb40fields_002.phpt"]="php84-fb3-dev:firebird40"
    ["tests/fbird_field_info_005.phpt"]="php84-fb3-dev:firebird40"
    ["tests/long_names_002.phpt"]="php83-dev:firebird30"
)
```

### Command Examples

```bash
# Full matrix (all combinations)
./scripts/test_matrix.sh --matrix

# Validate that skipped tests have working configurations
./scripts/test_matrix.sh --validate-skips

# List available combinations
./scripts/test_matrix.sh --list-combinations

# Run specific combination (existing functionality)
./scripts/test_matrix.sh php84-fb3-dev firebird40

# Run specific test with specific combination (existing functionality)
./scripts/test_matrix.sh php84-fb3-dev firebird40 tests/fb40fields_002.phpt
```
