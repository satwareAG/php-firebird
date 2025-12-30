#!/bin/bash
# run-valgrind.sh - Run tests with Valgrind memory error detection
#
# Purpose:
# Complementary to ASan for detecting uninitialized memory reads
# (a class of bugs ASan cannot catch)
#
# Usage:
#   ./scripts/run-valgrind.sh              # Run all tests
#   ./scripts/run-valgrind.sh tests/002.phpt  # Run specific test
#   ./scripts/run-valgrind.sh --quick      # Run subset for CI
#
# See: docs/research/asan-vs-valgrind-php-extensions.md

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log() { echo -e "[$(date +%H:%M:%S)] $*"; }
log_info() { echo -e "${GREEN}[INFO]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*"; }

# Required environment variables for PHP + Valgrind
export USE_ZEND_ALLOC=0
export ZEND_DONT_UNLOAD_MODULES=1
export NO_INTERACTION=1
export REPORT_EXIT_STATUS=1

# Parse arguments
QUICK_MODE=false
TEST_PATH=""
VALGRIND_LOG=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --quick)
            QUICK_MODE=true
            shift
            ;;
        --log)
            VALGRIND_LOG="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS] [TEST_PATH]"
            echo ""
            echo "Options:"
            echo "  --quick    Run subset of tests (faster for CI)"
            echo "  --log FILE Write Valgrind output to file"
            echo "  -h, --help Show this help"
            echo ""
            echo "Examples:"
            echo "  $0                      # Run all tests"
            echo "  $0 tests/002.phpt       # Run specific test"
            echo "  $0 --quick              # Quick CI run"
            exit 0
            ;;
        *)
            TEST_PATH="$1"
            shift
            ;;
    esac
done

# Check Valgrind is installed
if ! command -v valgrind &> /dev/null; then
    log_error "Valgrind not found. Install with: apt-get install valgrind"
    exit 1
fi

# Check suppression file exists
SUPP_FILE="$PROJECT_ROOT/valgrind-php.supp"
if [[ ! -f "$SUPP_FILE" ]]; then
    log_error "Suppression file not found: $SUPP_FILE"
    exit 1
fi

log_info "Running Valgrind memory check"
log_info "USE_ZEND_ALLOC=0 (bypass Zend memory manager)"
log_info "ZEND_DONT_UNLOAD_MODULES=1 (keep modules for stack traces)"
echo ""

# Build Valgrind command
VALGRIND_CMD=(
    valgrind
    --tool=memcheck
    --leak-check=full
    --show-leak-kinds=all
    --track-origins=yes
    --suppressions="$SUPP_FILE"
    --error-exitcode=1
    --gen-suppressions=all
)

# Add log file if specified
if [[ -n "$VALGRIND_LOG" ]]; then
    VALGRIND_CMD+=(--log-file="$VALGRIND_LOG")
    log_info "Writing Valgrind output to: $VALGRIND_LOG"
fi

# Determine test command
if [[ -n "$TEST_PATH" ]]; then
    # Run specific test
    log_info "Running test: $TEST_PATH"
    "${VALGRIND_CMD[@]}" php "$PROJECT_ROOT/run-tests.php" -m "$TEST_PATH"
elif [[ "$QUICK_MODE" == "true" ]]; then
    # Quick mode: run subset of critical tests
    log_info "Quick mode: running subset of tests"
    QUICK_TESTS=(
        tests/002.phpt
        tests/003.phpt
        tests/fbird_connect_dpb_001.phpt
        tests/fbird_blob_001.phpt
        tests/fbird_commit_001.phpt
    )
    for test in "${QUICK_TESTS[@]}"; do
        if [[ -f "$PROJECT_ROOT/$test" ]]; then
            log_info "Testing: $test"
            "${VALGRIND_CMD[@]}" php "$PROJECT_ROOT/run-tests.php" -m "$PROJECT_ROOT/$test" || {
                log_error "Test failed: $test"
                exit 1
            }
        fi
    done
else
    # Run all tests
    log_info "Running all tests with Valgrind (this will be slow)"
    log_warn "Consider using --quick for CI or specifying individual tests"
    "${VALGRIND_CMD[@]}" php "$PROJECT_ROOT/run-tests.php" -m "$PROJECT_ROOT/tests/"
fi

log_info "Valgrind check complete"