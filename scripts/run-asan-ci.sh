#!/bin/bash
# SPDX-License-Identifier: PHP-3.01
# ASAN (AddressSanitizer) CI Script for PHP-Firebird Extension
#
# This script builds the extension with ASAN enabled and runs the test suite
# to detect memory errors including:
# - Use-after-free (UAF)
# - Buffer overflow/underflow
# - Memory leaks
# - Double-free
#
# Usage: ./scripts/run-asan-ci.sh [--quick|--full|--uaf-only]
#   --quick    Run only critical tests (faster CI)
#   --full     Run complete test suite (default)
#   --uaf-only Run only UAF regression tests

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXT_DIR="$(dirname "$SCRIPT_DIR")"

# ASAN configuration
export USE_ZEND_ALLOC=0
export ZEND_DONT_UNLOAD_MODULES=1
export ASAN_OPTIONS="detect_leaks=1:abort_on_error=1:halt_on_error=0:log_path=/tmp/asan.log:symbolize=1"
export LSAN_OPTIONS="suppressions=${EXT_DIR}/valgrind-php.supp"

# Default mode
MODE="${1:-full}"

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_dependencies() {
    log_info "Checking dependencies..."
    
    # Check for ASAN-capable compiler
    if ! gcc --version &>/dev/null && ! clang --version &>/dev/null; then
        log_error "GCC or Clang required for ASAN builds"
        exit 1
    fi
    
    # Check for PHP development headers
    if ! php-config --version &>/dev/null; then
        log_error "php-config not found. Install PHP development headers."
        exit 1
    fi
    
    # Check for phpize
    if ! command -v phpize &>/dev/null; then
        log_error "phpize not found. Install PHP development tools."
        exit 1
    fi
    
    log_info "Dependencies OK"
}

clean_build() {
    log_info "Cleaning previous build..."
    cd "$EXT_DIR"
    
    if [ -f Makefile ]; then
        make clean 2>/dev/null || true
    fi
    
    if [ -f configure ]; then
        phpize --clean 2>/dev/null || true
    fi
}

build_with_asan() {
    log_info "Building extension with ASAN..."
    cd "$EXT_DIR"
    
    phpize
    
    # Configure with ASAN flags
    # -g: Debug symbols
    # -O1: Minimal optimization (better stack traces)
    # -fno-omit-frame-pointer: Better stack traces
    # -fsanitize=address: Enable AddressSanitizer
    ./configure \
        CFLAGS="-g -O1 -fno-omit-frame-pointer -fsanitize=address" \
        LDFLAGS="-fsanitize=address"
    
    make -j"$(nproc)"
    
    log_info "Build complete"
}

run_tests() {
    local test_pattern="$1"
    local test_name="$2"
    
    log_info "Running $test_name..."
    cd "$EXT_DIR"
    
    # Clear any previous ASAN logs
    rm -f /tmp/asan.log* 2>/dev/null || true
    
    local test_result=0
    make test TESTS="$test_pattern" NO_INTERACTION=1 || test_result=$?
    
    # Check for ASAN errors in log files
    if ls /tmp/asan.log* 1>/dev/null 2>&1; then
        log_error "ASAN detected memory errors!"
        echo "=== ASAN Log Output ==="
        cat /tmp/asan.log* || true
        echo "======================="
        return 1
    fi
    
    if [ $test_result -ne 0 ]; then
        log_warn "Some tests failed (exit code: $test_result)"
        return $test_result
    fi
    
    log_info "$test_name passed"
    return 0
}

run_quick_tests() {
    log_info "Running quick test suite (critical tests only)..."
    
    # Core functionality tests
    run_tests "tests/002.phpt tests/003.phpt tests/004.phpt tests/005.phpt" "Core tests"
    
    # Connection tests
    run_tests "tests/fbird_connect*.phpt tests/fbird_close*.phpt" "Connection tests"
    
    # Transaction tests
    run_tests "tests/fbird_trans*.phpt tests/fbird_commit*.phpt tests/fbird_rollback*.phpt" "Transaction tests"
    
    # UAF regression tests
    run_tests "tests/uaf_*.phpt" "UAF regression tests"
}

run_full_tests() {
    log_info "Running full test suite..."
    run_tests "tests/" "Full test suite"
}

run_uaf_tests() {
    log_info "Running UAF regression tests only..."
    
    # Check if UAF tests exist
    if ! ls "$EXT_DIR"/tests/uaf_*.phpt 1>/dev/null 2>&1; then
        log_warn "No UAF test files found (tests/uaf_*.phpt)"
        log_info "Consider running with --full for complete tests"
        return 0
    fi
    
    run_tests "tests/uaf_*.phpt" "UAF regression tests"
}

print_summary() {
    local status=$1
    
    echo ""
    echo "=========================================="
    if [ $status -eq 0 ]; then
        log_info "ASAN CI: ALL TESTS PASSED"
    else
        log_error "ASAN CI: TESTS FAILED (exit code: $status)"
    fi
    echo "=========================================="
    echo ""
    echo "ASAN Configuration:"
    echo "  USE_ZEND_ALLOC=$USE_ZEND_ALLOC"
    echo "  ZEND_DONT_UNLOAD_MODULES=$ZEND_DONT_UNLOAD_MODULES"
    echo "  ASAN_OPTIONS=$ASAN_OPTIONS"
    echo ""
}

main() {
    local exit_status=0
    
    echo "=========================================="
    echo "PHP-Firebird ASAN CI Runner"
    echo "Mode: $MODE"
    echo "=========================================="
    
    check_dependencies
    clean_build
    build_with_asan
    
    case "$MODE" in
        --quick|-q)
            run_quick_tests || exit_status=$?
            ;;
        --uaf-only|-u)
            run_uaf_tests || exit_status=$?
            ;;
        --full|-f|*)
            run_full_tests || exit_status=$?
            ;;
    esac
    
    print_summary $exit_status
    
    return $exit_status
}

# Run main function
main "$@"
