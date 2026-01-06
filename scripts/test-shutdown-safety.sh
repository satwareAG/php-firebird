#!/usr/bin/env bash
# SPDX-License-Identifier: PHP-3.01
# SPDX-FileCopyrightText: satware AG
#
# Shutdown Safety Validation Script
# Runs shutdown tests with Valgrind and AddressSanitizer to detect memory issues.
#
# Usage:
#   ./scripts/test-shutdown-safety.sh [--valgrind|--asan|--all]
#
# Requirements:
# - Docker environment running (docker/docker-compose.yml)
# - Valgrind installed for --valgrind
# - PHP built with ASAN for --asan
#
# Exit codes:
#   0 - All tests passed
#   1 - Test failures detected
#   2 - Configuration/setup error

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Default settings
RUN_VALGRIND=false
RUN_ASAN=false
RUN_BASIC=true
VERBOSE=false

# Shutdown test files
SHUTDOWN_TESTS=(
    "tests/shutdown_resource_cleanup.phpt"
    "tests/shutdown_persistent_link.phpt"
    "tests/shutdown_nested_resources.phpt"
    "tests/fbird_pconnect_shutdown_001.phpt"
)

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --valgrind    Run tests under Valgrind"
    echo "  --asan        Run tests with AddressSanitizer"
    echo "  --all         Run both Valgrind and ASAN tests"
    echo "  --basic       Run basic tests only (default)"
    echo "  -v, --verbose Enable verbose output"
    echo "  -h, --help    Show this help"
    echo ""
    echo "Examples:"
    echo "  $0                      # Basic test run"
    echo "  $0 --valgrind           # Test with Valgrind"
    echo "  $0 --all --verbose      # Full test suite with verbose output"
}

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --valgrind)
            RUN_VALGRIND=true
            shift
            ;;
        --asan)
            RUN_ASAN=true
            shift
            ;;
        --all)
            RUN_VALGRIND=true
            RUN_ASAN=true
            shift
            ;;
        --basic)
            RUN_BASIC=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            exit 2
            ;;
    esac
done

cd "$PROJECT_ROOT"

# Check if tests exist
for test_file in "${SHUTDOWN_TESTS[@]}"; do
    if [[ ! -f "$test_file" ]]; then
        log_error "Test file not found: $test_file"
        exit 2
    fi
done

log_info "Shutdown Safety Validation"
log_info "=========================="

FAILED=0

# Basic test run
if [[ "$RUN_BASIC" == "true" ]]; then
    log_info "Running basic shutdown tests..."
    
    for test_file in "${SHUTDOWN_TESTS[@]}"; do
        test_name=$(basename "$test_file" .phpt)
        log_info "  Testing: $test_name"
        
        if docker compose exec -T php83-dev php run-tests.php -p php -q "$test_file" 2>/dev/null; then
            echo -e "    ${GREEN}✓ PASS${NC}"
        else
            echo -e "    ${RED}✗ FAIL${NC}"
            FAILED=1
        fi
    done
fi

# Valgrind test run
if [[ "$RUN_VALGRIND" == "true" ]]; then
    log_info "Running shutdown tests with Valgrind..."
    
    # Check if Valgrind is available
    if ! docker compose exec -T php83-dev which valgrind > /dev/null 2>&1; then
        log_warn "Valgrind not available in container, skipping"
    else
        for test_file in "${SHUTDOWN_TESTS[@]}"; do
            test_name=$(basename "$test_file" .phpt)
            log_info "  Valgrind: $test_name"
            
            # Extract PHP code from .phpt file and run under Valgrind
            # Using --error-exitcode=1 to fail on memory errors
            if docker compose exec -T php83-dev bash -c "
                cd /app && \
                valgrind --error-exitcode=1 \
                         --leak-check=full \
                         --show-leak-kinds=definite \
                         --suppressions=valgrind-php.supp \
                         --quiet \
                         php -d extension=firebird.so \
                         run-tests.php -p php -q '$test_file' 2>&1
            " > /dev/null 2>&1; then
                echo -e "    ${GREEN}✓ PASS (no memory errors)${NC}"
            else
                echo -e "    ${RED}✗ FAIL (memory errors detected)${NC}"
                FAILED=1
            fi
        done
    fi
fi

# ASAN test run
if [[ "$RUN_ASAN" == "true" ]]; then
    log_info "Running shutdown tests with AddressSanitizer..."
    
    # Check if ASAN build is available
    if ! docker compose exec -T php83-dev test -f /usr/local/bin/php-asan 2>/dev/null; then
        log_warn "ASAN PHP build not available, skipping"
        log_info "Build with ASAN: ./scripts/run-sanitizer.sh --build"
    else
        for test_file in "${SHUTDOWN_TESTS[@]}"; do
            test_name=$(basename "$test_file" .phpt)
            log_info "  ASAN: $test_name"
            
            if docker compose exec -T php83-dev bash -c "
                cd /app && \
                ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
                /usr/local/bin/php-asan \
                run-tests.php -p /usr/local/bin/php-asan -q '$test_file' 2>&1
            " > /dev/null 2>&1; then
                echo -e "    ${GREEN}✓ PASS (no ASAN errors)${NC}"
            else
                echo -e "    ${RED}✗ FAIL (ASAN errors detected)${NC}"
                FAILED=1
            fi
        done
    fi
fi

# Summary
echo ""
if [[ $FAILED -eq 0 ]]; then
    log_info "All shutdown safety tests passed! ✓"
    exit 0
else
    log_error "Some shutdown safety tests failed! ✗"
    exit 1
fi
