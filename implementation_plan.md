# Implementation Plan: CI Pipeline Fixes for php-firebird

[Overview]
Fix the remaining CI pipeline issues for the php-firebird extension: sanitizers workflow ASan runtime loading and issue23 test flakiness.

The php-firebird extension's GitHub Actions CI has three workflows. The coverage workflow passes, but the main workflow has a flaky test (issue23) and the sanitizers workflow fails due to ASan symbol resolution issues. This plan addresses both problems to achieve reliable CI.

**Current State:**
- `coverage.yml`: ✅ PASSES (56.8% coverage, threshold 55%)
- `main.yml`: ⚠️ 3/4 jobs pass (issue23 test fails on PHP 8.1 / FB 5.0)
- `sanitizers.yml`: ❌ FAILS (ASan symbols cannot resolve when loading extension)

**Root Causes Identified:**
1. **Sanitizers**: Extension compiled with ASan flags references symbols (`__asan_option_detect_stack_use_after_return`) that the PHP binary doesn't have. Solution: Use `LD_PRELOAD` to load the ASan runtime library before PHP starts.
2. **Issue23 Test**: The test queries system tables (`RDB$FIELDS`, `RDB$RELATION_FIELDS`) with `FIRST 1`. Row order from system tables can vary, and behavior differs between PHP 8.1 and 8.4 on the same Firebird server.

[Types]
No type definitions required for this implementation.

This is a CI/CD and test fix task that modifies GitHub Actions workflow YAML files and PHPT test files. No application code types are involved.

[Files]
Modify two workflow files and one test file.

**Files to Modify:**

1. `.github/workflows/sanitizers.yml`
   - Add `LD_PRELOAD` for ASan runtime in the "Verify extension and connection" step
   - Add `LD_PRELOAD` for ASan runtime in the "Run tests with sanitizers" step
   - Add `LD_PRELOAD` for LSan runtime in the LSan job's verification and test steps
   - Find and export the correct library paths at runtime

2. `tests/issue23_alias_padding_001.phpt`
   - Replace system table query with a controlled CTE-based query
   - Ensure deterministic output regardless of database state
   - Maintain the purpose of testing column alias deduplication with padding

**No New Files Required**

[Functions]
No function modifications required for this implementation.

This task modifies shell scripts embedded in YAML workflow files and PHP test scripts. No C functions or PHP library functions are changed.

[Classes]
No class modifications required for this implementation.

This is a CI/CD infrastructure fix with no object-oriented code changes.

[Dependencies]
No dependency changes required.

The sanitizers workflow already installs `libasan8`, `libubsan1`, and `liblsan0` packages. No additional packages are needed - the fix is to preload these libraries at runtime.

[Testing]
Test by pushing changes and verifying CI workflows pass.

**Verification Steps:**
1. Push changes to a branch
2. Monitor GitHub Actions workflows
3. Verify `sanitizers.yml` jobs complete successfully:
   - ASan + UBSan (PHP 8.3) job should pass
   - LeakSanitizer (PHP 8.3) job should pass
4. Verify `main.yml` all 4 matrix jobs pass:
   - PHP 8.1 / Firebird 3.0
   - PHP 8.1 / Firebird 5.0
   - PHP 8.4 / Firebird 3.0
   - PHP 8.4 / Firebird 5.0
5. Verify `coverage.yml` continues to pass

**Expected Outcomes:**
- Sanitizers workflow: All jobs green
- Main workflow: All 4 matrix combinations pass
- Coverage workflow: Continues to pass with >55% coverage

[Implementation Order]
Fix sanitizers first (more complex), then fix the flaky test. Local test script already updated.

**Step 0: Update Local Test Script (COMPLETED)**
Updated `scripts/test_with_act.sh` to use GCC instead of clang and add LD_PRELOAD for ASan runtime, matching the CI changes to be made.

**Step 1: Fix Sanitizers Workflow - ASan Job**
Modify `.github/workflows/sanitizers.yml` to detect and preload the ASan runtime library before running PHP commands in the `asan-ubsan` job.

Add before PHP invocations in "Verify extension and connection" step:
```bash
# Find and preload ASan runtime for GCC-built sanitized extension
ASAN_LIB=$(find /usr/lib -name "libasan.so.*" -type f 2>/dev/null | head -1)
if [ -n "$ASAN_LIB" ]; then
  export LD_PRELOAD="$ASAN_LIB"
  echo "Preloading ASan runtime: $ASAN_LIB"
fi
```

**Step 2: Fix Sanitizers Workflow - LSan Job**
Apply similar `LD_PRELOAD` fix to the LeakSanitizer job, preloading `liblsan.so`:
```bash
# Find and preload LSan runtime for GCC-built sanitized extension
LSAN_LIB=$(find /usr/lib -name "liblsan.so.*" -type f 2>/dev/null | head -1)
if [ -n "$LSAN_LIB" ]; then
  export LD_PRELOAD="$LSAN_LIB"
  echo "Preloading LSan runtime: $LSAN_LIB"
fi
```

**Step 3: Fix Issue23 Test**
Replace the system table query with a deterministic CTE-based query that:
- Creates duplicate column names explicitly
- Does not depend on database state
- Produces consistent output across PHP versions

New query approach:
```sql
WITH DUP AS (SELECT 'VALUE1' AS COL_NAME, 'VALUE2' AS COL_NAME FROM RDB$DATABASE)
SELECT * FROM DUP
```

**Step 4: Commit and Push**
- Commit changes with descriptive message following Conventional Commits
- Push to `satware-main` branch
- Monitor CI workflows for all-green status

**Step 5: Update CHANGELOG**
- Document the CI fixes in `CHANGELOG.md` under version 7.0.0-rc.12 or create new section if releasing

**Step 6: Verify Final State**
- Confirm all three workflows pass
- Document any remaining known issues
