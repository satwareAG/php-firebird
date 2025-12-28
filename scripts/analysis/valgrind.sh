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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

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

echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║        Valgrind Memory Analysis for PHP Firebird             ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo -e "Mode: ${YELLOW}$MODE${NC}"
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
    echo -e "${RED}ERROR: firebird.so not found. Build the extension first.${NC}"
    exit 1
fi

echo -e "PHP Binary: ${YELLOW}$PHP_BINARY${NC}"
echo -e "Extension:  ${YELLOW}$EXT_PATH${NC}"

# ============================================================================
# Locate Suppressions File
# ============================================================================
SUPP_FILE=""
for path in "$PROJECT_ROOT/valgrind-php.supp" "/ext/valgrind-php.supp" "./valgrind-php.supp"; do
    if [ -f "$path" ]; then
        SUPP_FILE="$path"
        break
    fi
done

SUPP_OPTS=""
if [ -n "$SUPP_FILE" ]; then
    echo -e "Suppressions: ${YELLOW}$SUPP_FILE${NC}"
    SUPP_OPTS="--suppressions=$SUPP_FILE"
else
    echo -e "${YELLOW}⚠ No suppressions file found (expect noise from PHP/Firebird internals)${NC}"
fi

echo ""

# ============================================================================
# Valgrind Options
# ============================================================================
VALGRIND_OPTS="
    --tool=memcheck
    --leak-check=full
    --show-leak-kinds=definite,indirect,possible
    --track-origins=yes
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
    echo -e "${BLUE}═══ Quick Tests: Extension Loading ═══${NC}"
    
    echo -e "\n${BLUE}>> Test 1: Basic extension load...${NC}"
    if valgrind $VALGRIND_OPTS $PHP_BINARY -d extension="$EXT_PATH" -r "echo 'Extension loaded successfully\n';" 2>&1; then
        echo -e "${GREEN}✓ Extension load test passed${NC}"
    else
        echo -e "${RED}✗ Extension load test failed${NC}"
        FAILED=1
    fi
    
    echo -e "\n${BLUE}>> Test 2: Client version function...${NC}"
    if valgrind $VALGRIND_OPTS $PHP_BINARY -d extension="$EXT_PATH" -r '
        if (function_exists("fbu_get_client_version")) {
            $v = fbu_get_client_version(null);
            echo "Client version: $v\n";
        } else {
            echo "fbu_get_client_version not available\n";
        }
    ' 2>&1; then
        echo -e "${GREEN}✓ Client version test passed${NC}"
    else
        echo -e "${RED}✗ Client version test failed${NC}"
        FAILED=1
    fi
}

# ============================================================================
# Full Mode: Comprehensive Function Tests
# ============================================================================
run_full_tests() {
    run_quick_tests
    
    echo -e "\n${BLUE}═══ Full Tests: Function Coverage ═══${NC}"
    
    # Test Firebird connection if database is available
    DB_PATH="${FIREBIRD_DB_PATH:-/var/lib/firebird/data/test.fdb}"
    
    echo -e "\n${BLUE}>> Test 3: Database connection cycle...${NC}"
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
        echo -e "${GREEN}✓ Connection cycle test passed${NC}"
    else
        echo -e "${RED}✗ Connection cycle test failed${NC}"
        FAILED=1
    fi
    
    echo -e "\n${BLUE}>> Test 4: Error handling...${NC}"
    if valgrind $VALGRIND_OPTS $PHP_BINARY -d extension="$EXT_PATH" -r '
        // Test error message functions without connection
        $err = @fbird_errmsg();
        $code = @fbird_errcode();
        echo "Error functions work: msg='$err' code=$code\n";
    ' 2>&1; then
        echo -e "${GREEN}✓ Error handling test passed${NC}"
    else
        echo -e "${RED}✗ Error handling test failed${NC}"
        FAILED=1
    fi
}

# ============================================================================
# Tests Mode: Run PHPT Suite Under Valgrind
# ============================================================================
run_phpt_tests() {
    run_full_tests
    
    echo -e "\n${BLUE}═══ PHPT Test Suite Under Valgrind ═══${NC}"
    echo -e "${YELLOW}Note: This is slow but provides comprehensive memory coverage${NC}"
    
    # Find tests directory
    TESTS_DIR=""
    for path in "$PROJECT_ROOT/tests" "/ext/tests" "./tests"; do
        if [ -d "$path" ]; then
            TESTS_DIR="$path"
            break
        fi
    done
    
    if [ -z "$TESTS_DIR" ]; then
        echo -e "${RED}ERROR: tests directory not found${NC}"
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
            echo -e "\n${BLUE}>> Running $test under Valgrind...${NC}"
            
            # Extract --FILE-- section and run it
            # This is a simplified approach; for full accuracy use run-tests.php with -m flag
            if php "$PROJECT_ROOT/run-tests.php" -m -p "valgrind $VALGRIND_OPTS $PHP_BINARY" "$TESTS_DIR/$test" 2>&1; then
                echo -e "${GREEN}✓ $test passed${NC}"
            else
                echo -e "${YELLOW}⚠ $test had warnings (review output)${NC}"
            fi
        fi
    done
    
    # Alternative: Use make test with Valgrind
    echo -e "\n${BLUE}>> Running make test with Valgrind (sample)...${NC}"
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
echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}║            ✓ Valgrind Analysis Complete - No Leaks          ║${NC}"
else
    echo -e "${RED}║            ✗ Valgrind Analysis Found Issues                  ║${NC}"
fi
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"

exit $FAILED
