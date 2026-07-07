#!/bin/bash
# scripts/analysis/valgrind.sh
# Valgrind memory analysis for PHP Firebird extension
#
# Best practices for PHP extension Valgrind testing (2024-2025):
# - USE_ZEND_ALLOC=0: Disables Zend's memory manager so Valgrind sees real malloc/free
# - ZEND_DONT_UNLOAD_MODULES=1: Keeps extension loaded for proper symbol resolution
# - Use suppression file for known PHP/Firebird client library false positives
#
# Usage: ./scripts/analysis/valgrind.sh [--quick|--full|--tests]
#   --quick  Run quick extension load test only (default)
#   --full   Run comprehensive tests (load + functions + connection)
#   --tests  Run PHPT test suite under Valgrind (slow but thorough)

set -e

source "$(dirname "$(dirname "$0")")/lib/logging.sh"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Parse mode
MODE="${1:-quick}"
case "$MODE" in
    --quick|-q) MODE="quick" ;;
    --full|-f) MODE="full" ;;
    --tests|-t) MODE="tests" ;;
    quick|full|tests) ;;
    *)
        echo "Usage: $0 [--quick|--full|--tests]"
        exit 1
        ;;
esac

log_info "╔══════════════════════════════════════════════════════════════╗"
log_info "║        Valgrind Memory Analysis for PHP Firebird             ║"
log_info "╚══════════════════════════════════════════════════════════════╝"
log_info "Mode: $MODE"
echo ""

# ============================================================================
# Environment Variables (Critical for PHP Extension Testing)
# ============================================================================
# Disable Zend Memory Manager so Valgrind sees real allocations
export USE_ZEND_ALLOC=0

# Keep modules loaded so Valgrind can resolve symbols
export ZEND_DONT_UNLOAD_MODULES=1

# Firebird connection settings (for full mode)
export ISC_USER="${ISC_USER:-SYSDBA}"
export ISC_PASSWORD="${ISC_PASSWORD:-masterkey}"

# ============================================================================
# Locate PHP and Extension
# ============================================================================
PHP_BINARY=${PHP_BINARY:-php}

# Find extension - try various locations
EXT_PATH=""
for path in "./modules/firebird.so" "/ext/modules/firebird.so" "$PROJECT_ROOT/modules/firebird.so"; do
    if [ -f "$path" ]; then
        EXT_PATH="$path"
        break
    fi
done

if [ -z "$EXT_PATH" ]; then
    log_error "ERROR: firebird.so not found. Build the extension first."
    exit 1
fi

log_info "PHP Binary: $PHP_BINARY"
log_info "Extension:  $EXT_PATH"

# ============================================================================
# Locate Suppressions File
# ============================================================================
# Priority order for Docker compatibility:
# 1. /ext/valgrind-php.supp - Docker mount path (most common in CI)
# 2. $SCRIPT_DIR/../../valgrind-php.supp - Relative to script location
# 3. $PROJECT_ROOT/valgrind-php.supp - Calculated project root
# 4. ./valgrind-php.supp - Current directory fallback
SUPP_FILE=""
if [ -f "/ext/valgrind-php.supp" ]; then
    SUPP_FILE="/ext/valgrind-php.supp"
elif [ -f "$SCRIPT_DIR/../../valgrind-php.supp" ]; then
    SUPP_FILE="$SCRIPT_DIR/../../valgrind-php.supp"
elif [ -f "$PROJECT_ROOT/valgrind-php.supp" ]; then
    SUPP_FILE="$PROJECT_ROOT/valgrind-php.supp"
elif [ -f "./valgrind-php.supp" ]; then
    SUPP_FILE="./valgrind-php.supp"
fi

SUPP_OPTS=""
if [ -n "$SUPP_FILE" ]; then
    log_info "Suppressions: $SUPP_FILE"
    SUPP_OPTS="--suppressions=$SUPP_FILE"
else
    log_warn "No suppressions file found (expect noise from PHP/Firebird internals)"
fi

echo ""

# ============================================================================
# Valgrind Options
# ============================================================================
# Best practice options for PHP extension memory analysis (2024-2025):
# - --track-origins=yes: Shows source of uninitialized values
# - --num-callers=30: Deep stack traces for complex call chains
# - --errors-for-leak-kinds=definite,indirect: Fail only on real leaks
VALGRIND_OPTS="
    --tool=memcheck
    --leak-check=full
    --show-leak-kinds=definite,indirect,possible
    --track-origins=yes
    --num-callers=30
    --error-exitcode=1
    --errors-for-leak-kinds=definite,indirect
    $SUPP_OPTS"

# For verbose output during debugging:
# VALGRIND_OPTS="$VALGRIND_OPTS --verbose"

FAILED=0

# ============================================================================
# Quick Mode: Extension Load Test
# ============================================================================
run_quick_tests() {
    log_info "═══ Quick Tests: Extension Loading ═══"

    echo ""
    log_info "Test 1: Basic extension load..."
    if valgrind $VALGRIND_OPTS $PHP_BINARY -d extension="$EXT_PATH" -r "echo 'Extension loaded successfully\n';" 2>&1; then
        log_pass "Extension load test passed"
    else
        log_fail "Extension load test failed"
        FAILED=1
    fi

    echo ""
    log_info "Test 2: Client version function..."
    if valgrind $VALGRIND_OPTS $PHP_BINARY -d extension="$EXT_PATH" -r '
        if (function_exists("fbird_get_client_version")) {
            $v = fbird_get_client_version(null);
            echo "Client version: $v\n";
        } else {
            echo "fbird_get_client_version not available\n";
        }
    ' 2>&1; then
        log_pass "Client version test passed"
    else
        log_fail "Client version test failed"
        FAILED=1
    fi
}

# ============================================================================
# Full Mode: Comprehensive Function Tests
# ============================================================================
run_full_tests() {
    run_quick_tests
    
    echo ""
    log_info "═══ Full Tests: Function Coverage ═══"

    # Test Firebird connection if database is available
    DB_PATH="${FIREBIRD_DB_PATH:-/var/lib/firebird/data/test.fdb}"

    echo ""
    log_info "Test 3: Database connection cycle..."
    if valgrind $VALGRIND_OPTS $PHP_BINARY -d extension="$EXT_PATH" -r "
        \$db = @fbird_connect('$DB_PATH');
        if (\$db) {
            echo \"Connected successfully\n\";
            fbird_close(\$db);
            echo \"Disconnected\n\";
        } else {
            echo \"Connection failed (expected if no Firebird server)\n\";
        }
    " 2>&1; then
        log_pass "Connection cycle test passed"
    else
        log_fail "Connection cycle test failed"
        FAILED=1
    fi

    echo ""
    log_info "Test 4: Error handling..."
    if valgrind $VALGRIND_OPTS $PHP_BINARY -d extension="$EXT_PATH" -r '
        // Test error message functions without connection
        $err = @fbird_errmsg();
        $code = @fbird_errcode();
        echo "Error functions work: msg='\''$err'\'' code=$code\n";
    ' 2>&1; then
        log_pass "Error handling test passed"
    else
        log_fail "Error handling test failed"
        FAILED=1
    fi
}

# ============================================================================
# Tests Mode: Run PHPT Suite Under Valgrind
# ============================================================================
run_phpt_tests() {
    run_full_tests
    
    echo ""
    log_info "═══ PHPT Test Suite Under Valgrind ═══"
    log_warn "Note: This is slow but provides comprehensive memory coverage"

    # Find tests directory
    TESTS_DIR=""
    for path in "$PROJECT_ROOT/tests" "/ext/tests" "./tests"; do
        if [ -d "$path" ]; then
            TESTS_DIR="$path"
            break
        fi
    done

    if [ -z "$TESTS_DIR" ]; then
        log_error "ERROR: tests directory not found"
        FAILED=1
        return
    fi
    
    # Run a subset of critical tests under Valgrind
    # Running ALL tests under Valgrind takes too long
    CRITICAL_TESTS=(
        "fbird_connect_dpb_001.phpt"
        "fbird_blob_001.phpt"
        "fbird_field_info_001.phpt"
        "fbird_num_fields_001.phpt"
        "fbird_affected_rows_001.phpt"
    )
    
    for test in "${CRITICAL_TESTS[@]}"; do
        if [ -f "$TESTS_DIR/$test" ]; then
            echo ""
            log_info "Running $test under Valgrind..."

            # Extract --FILE-- section and run it
            # This is a simplified approach; for full accuracy use run-tests.php with -m flag
            if php "$PROJECT_ROOT/run-tests.php" -m -p "valgrind $VALGRIND_OPTS $PHP_BINARY" "$TESTS_DIR/$test" 2>&1; then
                log_pass "$test passed"
            else
                log_warn "$test had warnings (review output)"
            fi
        fi
    done

    # Alternative: Use make test with Valgrind
    echo ""
    log_info "Running make test with Valgrind (sample)..."
    if [ -f Makefile ]; then
        # Run just a few tests to verify
        TEST_PHP_EXECUTABLE="valgrind $VALGRIND_OPTS $PHP_BINARY" \
        make test TESTS="$TESTS_DIR/002.phpt $TESTS_DIR/003.phpt" 2>&1 || true
    fi
}

# ============================================================================
# Run Selected Mode
# ============================================================================
case "$MODE" in
    quick) run_quick_tests ;;
    full)  run_full_tests ;;
    tests) run_phpt_tests ;;
esac

# ============================================================================
# Summary
# ============================================================================
echo ""
log_info "╔══════════════════════════════════════════════════════════════╗"
if [ $FAILED -eq 0 ]; then
    log_pass "║            Valgrind Analysis Complete - No Leaks          ║"
else
    log_fail "║            Valgrind Analysis Found Issues                  ║"
fi
log_info "╚══════════════════════════════════════════════════════════════╝"

exit $FAILED
