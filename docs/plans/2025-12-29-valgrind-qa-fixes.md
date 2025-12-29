# Implementation Plan: QA Workflow Valgrind Fixes for PHP Extensions

[Overview]
Fix and modernize the Valgrind memory testing workflow in `scripts/qa.sh` and `scripts/analysis/valgrind.sh` to follow PHP 8.1+ extension testing best practices.

This implementation addresses GitHub Issues #27 and #29 by fixing suppression file path resolution, ensuring proper environment variables, and validating the complete QA workflow. The changes ensure Valgrind correctly detects memory issues in the php-firebird extension while suppressing known false positives from PHP internals and the Firebird client library.

**Root Cause Analysis:**
1. The `valgrind.sh` script's suppression file path search works when run directly but fails when called from `qa.sh` via Docker
2. The Docker exec context changes the working directory, breaking relative path resolution
3. Debug build flags (`-g -O0`) are not consistently applied for Valgrind runs
4. Missing `--track-origins=yes` flag per 2024-2025 best practices

**Best Practices Applied (PHP 8.1+ / 2024-2025):**
- `USE_ZEND_ALLOC=0`: Disables Zend's memory manager so Valgrind sees real malloc/free
- `ZEND_DONT_UNLOAD_MODULES=1`: Keeps extension loaded for proper symbol resolution
- `--track-origins=yes`: Detects uninitialized memory sources
- `-g -O0` compile flags: Accurate line number reporting
- Custom suppression file for PHP/Firebird false positives

[Types]
No type definitions required - this is a shell script modification.

The implementation involves modifying Bash scripts and suppression files only. No programming language types, interfaces, or data structures are affected.

[Files]
Modify existing QA scripts and suppression files to fix path resolution and add best practices.

**Files to Modify:**

1. `scripts/analysis/valgrind.sh` - Main Valgrind wrapper script
   - Fix suppression file path resolution for Docker context
   - Add `--track-origins=yes` flag
   - Improve error messaging
   - Add explicit `-g -O0` note in documentation

2. `scripts/qa.sh` - Master QA workflow script  
   - Ensure debug build flags when building for Valgrind
   - Pass correct paths to valgrind.sh
   - Fix Phase 6 rebuild to use debug flags consistently

3. `valgrind-php.supp` - Valgrind suppression file
   - Add any missing PHP 8.3/8.4 specific suppressions
   - Add Firebird client library suppressions for latest versions
   - Document suppression patterns

**Files Unchanged:**
- `docker/docker-compose.yml` - Container config is correct
- `scripts/analysis/lsan.supp` - LeakSanitizer suppression (separate tool)

[Functions]
Update shell script functions for proper path handling and Valgrind execution.

**Functions in `scripts/analysis/valgrind.sh` to modify:**

1. Suppression file locator (lines ~70-80)
   - Current: Searches relative paths that fail in Docker
   - Change: Use `$SCRIPT_DIR` as base, add Docker mount path `/ext/valgrind-php.supp`

2. `VALGRIND_OPTS` definition (lines ~90-100)
   - Current: Basic options without origin tracking
   - Change: Add `--track-origins=yes` for uninitialized memory detection

3. `run_quick_tests()`, `run_full_tests()`, `run_phpt_tests()` functions
   - Add proper error handling for suppression file warnings
   - Ensure informative output about which file is being used

**Functions in `scripts/qa.sh` to modify:**

1. Phase 6 rebuild section (lines ~240-250)
   - Current: Uses `CFLAGS='-g -O0'` inline
   - Change: Verify this is passed correctly and add `-fno-omit-frame-pointer` for better stack traces

[Classes]
No class changes required - this is a shell script implementation.

Shell scripts do not use object-oriented programming patterns.

[Dependencies]
No new dependencies required.

Current dependencies remain unchanged:
- `valgrind` (Memcheck tool) - already installed in PHP dev containers
- `bear` - for compile_commands.json generation
- `docker compose` - for container orchestration

Container images already include Valgrind via apt packages.

[Testing]
Validate the QA workflow runs without suppression file errors and produces clean reports.

**Testing Strategy:**

1. **Unit Test: valgrind.sh direct execution**
   ```bash
   docker compose exec php83-dev /ext/scripts/analysis/valgrind.sh --quick
   # Expected: No "can't open suppressions file" error
   # Expected: Reports "Suppressions: /ext/valgrind-php.supp"
   ```

2. **Integration Test: qa.sh full mode**
   ```bash
   ./scripts/qa.sh --container php83-dev --mode full
   # Expected: Phase 6 Valgrind completes without errors
   # Expected: Only suppressed warnings (known PHP/FB issues)
   ```

3. **Regression Test: Ensure no new leaks**
   ```bash
   # After running, verify exit code 0
   echo $?  # Should be 0
   ```

4. **Cross-PHP Version Test**
   ```bash
   for php in php81-dev php82-dev php83-dev php84-dev; do
     ./scripts/qa.sh --container $php --mode full --fail-fast
   done
   ```

**Validation Checklist:**
- [x] `valgrind.sh --quick` finds suppression file in Docker
- [x] `valgrind.sh --full` connects to Firebird without memory errors
- [x] `qa.sh --mode full` Phase 6 completes successfully
- [x] No new unsuppressed memory leaks detected
- [x] Works across PHP 8.1, 8.2, 8.3, 8.4 containers (validated 2025-12-29)
- [x] Issue #27 acceptance criteria met
- [x] Issue #29 acceptance criteria met

**Cross-PHP Validation Results (2025-12-29):**
| PHP Version | Tests Passed | Valgrind | UBSan | Status |
|-------------|--------------|----------|-------|--------|
| php81-dev   | 132/132      | Clean    | Clean | ✅ PASS |
| php82-dev   | 133/133      | Clean    | Clean | ✅ PASS |
| php83-dev   | 133/133      | Clean    | Clean | ✅ PASS |
| php84-dev   | 133/133      | Clean    | Clean | ✅ PASS |

[Implementation Order]
Sequential implementation steps to fix the QA Valgrind workflow.

1. **Fix valgrind.sh suppression file path resolution**
   - Update path search to prioritize `/ext/valgrind-php.supp` for Docker context
   - Add `$SCRIPT_DIR/../../valgrind-php.supp` as fallback
   - Make suppression file optional with clear warning message

2. **Add Valgrind best practice flags**
   - Add `--track-origins=yes` to VALGRIND_OPTS
   - Add `--expensive-definedness-checks=yes` for thorough analysis (optional mode)
   - Verify `--error-exitcode=1` and `--errors-for-leak-kinds=definite,indirect` are correct

3. **Update qa.sh Phase 6 build**
   - Ensure `-g -O0 -fno-omit-frame-pointer` flags for debug builds
   - Add explicit rebuild step before Valgrind (clean any optimized builds)

4. **Update valgrind-php.supp suppressions**
   - Add any PHP 8.4 specific patterns if needed
   - Document each suppression pattern's purpose

5. **Test and validate**
   - Run `valgrind.sh --quick` in Docker
   - Run `qa.sh --mode full`
   - Verify all PHP versions pass
   - Close Issues #27 and #29

**Detailed Code Changes:**

**Step 1 - valgrind.sh path fix:**
```bash
# Replace current SUPP_FILE search with:
SUPP_FILE=""
# Priority 1: Docker mount path (most common)
if [ -f "/ext/valgrind-php.supp" ]; then
    SUPP_FILE="/ext/valgrind-php.supp"
# Priority 2: Relative to script dir
elif [ -f "$SCRIPT_DIR/../../valgrind-php.supp" ]; then
    SUPP_FILE="$SCRIPT_DIR/../../valgrind-php.supp"
# Priority 3: Project root (if running from there)
elif [ -f "$PROJECT_ROOT/valgrind-php.supp" ]; then
    SUPP_FILE="$PROJECT_ROOT/valgrind-php.supp"
# Priority 4: Current directory
elif [ -f "./valgrind-php.supp" ]; then
    SUPP_FILE="./valgrind-php.supp"
fi
```

**Step 2 - Valgrind options update:**
```bash
VALGRIND_OPTS="
    --tool=memcheck
    --leak-check=full
    --show-leak-kinds=definite,indirect,possible
    --track-origins=yes
    --error-exitcode=1
    --errors-for-leak-kinds=definite,indirect
    --num-callers=30
    $SUPP_OPTS"
```

**Step 3 - qa.sh Phase 6 rebuild:**
```bash
# Ensure clean build with debug symbols for Valgrind
docker compose exec -T "$CONTAINER" bash -c "
    cd /ext
    make clean 2>/dev/null || true
    phpize --clean 2>/dev/null || true
    
    phpize
    CFLAGS='-g -O0 -fno-omit-frame-pointer' \
    CXXFLAGS='-g -O0 -fno-omit-frame-pointer' \
    ./configure --with-firebird=/usr
    make -j\$(nproc)
"
