#!/bin/bash
# scripts/analysis/sanitizers.sh
# Comprehensive sanitizer testing: AddressSanitizer + UndefinedBehaviorSanitizer + LeakSanitizer
# Usage: ./sanitizers.sh [options] [mode]
#   mode: 'asan' (default), 'ubsan', 'all'
#   options:
#     --test NAME   Run specific test (e.g. asan_basic)
#     --list        List available tests
#     --verbose     Show full output
#     --help        Show help

set -e

source "$(dirname "$(dirname "$0")")/lib/logging.sh"

# Defaults
MODE="asan"
SPECIFIC_TEST=""
VERBOSE=false
LIST_TESTS=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --test)
            SPECIFIC_TEST="$2"
            shift 2
            ;;
        --list)
            LIST_TESTS=true
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        --help)
            echo "Usage: $0 [options] [mode]"
            echo "Modes: asan (default), ubsan, all"
            echo "Options:"
            echo "  --test NAME   Run specific test (e.g. asan_basic)"
            echo "  --list        List available tests"
            echo "  --verbose     Show full output"
            exit 0
            ;;
        asan|ubsan|all)
            MODE="$1"
            shift
            ;;
        *)
            echo "Unknown argument: $1"
            exit 1
            ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXT_DIR="${SCRIPT_DIR}/../.."

cd "$EXT_DIR"

# Define tests
SAN_TESTS=(
    "tests/sanitizer/asan_basic.php"
    "tests/sanitizer/blob_operations.php"
    "tests/sanitizer/transaction_stress.php"
)

if [ "$LIST_TESTS" = true ]; then
    echo "Available sanitizer tests:"
    for test in "${SAN_TESTS[@]}"; do
        echo "  - $(basename "$test" .php)"
    done
    exit 0
fi

log_info "=== Sanitizer Testing Suite ==="
echo "Mode: $MODE"
if [ -n "$SPECIFIC_TEST" ]; then
    echo "Target: $SPECIFIC_TEST"
fi

# Common sanitizer flags
COMMON_FLAGS="-fno-omit-frame-pointer -g -O1"
ASAN_FLAGS="-fsanitize=address,leak"
# GCC compatibility: remove nullability (Clang only) and integer (too broad/noisy or Clang specific)
# We stick to standard undefined behavior checks which are supported by both GCC and Clang
UBSAN_FLAGS="-fsanitize=undefined -fno-sanitize-recover=all"

# Check if we are in the ASan container
IS_ASAN_CONTAINER=false
if [ -n "$USE_ZEND_ALLOC" ] && [ "$USE_ZEND_ALLOC" -eq 0 ]; then
    IS_ASAN_CONTAINER=true
    log_warn "Detected ASan container environment"
fi

# Safety cleanup function
cleanup_build() {
    if [ -f Makefile ]; then
        make clean 2>/dev/null || true
        phpize --clean 2>/dev/null || true
    fi
    # Force remove artifacts that might be owned by root if we are running as user
    rm -rf modules/firebird.so .libs/ .deps/ build/ autom4te.cache/ 2>/dev/null || true
}

run_asan_build() {
    echo ""
    log_info "Building with AddressSanitizer + LeakSanitizer..."

    cleanup_build

    phpize

    # Configure with ASan flags
    # If in ASan container, PHP is already built with ASan, so we just need to match flags
    # and ensure we don't use LD_PRELOAD later
    CFLAGS="$COMMON_FLAGS $ASAN_FLAGS" \
    CXXFLAGS="$COMMON_FLAGS $ASAN_FLAGS" \
    LDFLAGS="$ASAN_FLAGS" \
    ./configure --with-firebird=/usr

    make -j"$(nproc)"

    log_pass "ASan build complete"
}

run_ubsan_build() {
    echo ""
    log_info "Building with UndefinedBehaviorSanitizer..."

    cleanup_build

    phpize

    # Configure with UBSan flags
    CFLAGS="$COMMON_FLAGS $UBSAN_FLAGS" \
    CXXFLAGS="$COMMON_FLAGS $UBSAN_FLAGS" \
    LDFLAGS="$UBSAN_FLAGS" \
    ./configure --with-firebird=/usr

    make -j"$(nproc)"

    log_pass "UBSan build complete"
}

run_tests_with_sanitizer() {
    local sanitizer_name=$1

    echo ""
    log_info "Running tests with $sanitizer_name..."

    # Sanitizer runtime options (PHP-src compatible patterns)
    # exitcode=139 (128+11=SIGSEGV): PHP-src CI convention for ASan detection
    # exitcode=138 (128+10=SIGBUS): UBSAN exit code for CI detection
    # abort_on_error=0: Don't abort, use exitcode instead for better CI integration
    export ASAN_OPTIONS="exitcode=139:abort_on_error=0:detect_leaks=1:check_initialization_order=1:strict_init_order=1:detect_stack_use_after_return=1:halt_on_error=0"
    export UBSAN_OPTIONS="exitcode=138:print_stacktrace=1:halt_on_error=0"
    export LSAN_OPTIONS="suppressions=/ext/scripts/analysis/lsan.supp:print_suppressions=0"

    # Find llvm-symbolizer for better stack traces
    if [ -f /usr/bin/llvm-symbolizer ]; then
        export ASAN_SYMBOLIZER_PATH="/usr/bin/llvm-symbolizer"
    elif [ -f /usr/lib/llvm-14/bin/llvm-symbolizer ]; then
        export ASAN_SYMBOLIZER_PATH="/usr/lib/llvm-14/bin/llvm-symbolizer"
    fi

    # Verify extension loads
    echo "Verifying extension loads..."
    
    # If NOT in ASan container, we might need LD_PRELOAD (which is problematic)
    # But if we ARE in ASan container, we run directly.
    
    VERIFY_OUTPUT=$(php -d extension=./modules/firebird.so -m 2>&1)
    if ! echo "$VERIFY_OUTPUT" | grep -q "firebird"; then
        log_fail "Extension failed to load"
        echo "Output:"
        echo "$VERIFY_OUTPUT"
        
        if [ "$IS_ASAN_CONTAINER" = false ] && [ "$sanitizer_name" = "AddressSanitizer" ]; then
             log_warn "Hint: You are running ASan tests outside the ASan container."
             log_warn "      This often fails due to RTLD_DEEPBIND conflicts."
             log_warn "      Use 'scripts/run-sanitizer.sh' to run in the correct environment."
        fi
        exit 1
    fi
    log_pass "Extension loaded successfully"

    local FAILED=0
    
    # Filter tests if specific test requested
    local TESTS_TO_RUN=()
    if [ -n "$SPECIFIC_TEST" ]; then
        for test in "${SAN_TESTS[@]}"; do
            if [[ "$test" == *"$SPECIFIC_TEST"* ]]; then
                TESTS_TO_RUN+=("$test")
            fi
        done
        if [ ${#TESTS_TO_RUN[@]} -eq 0 ]; then
            log_fail "Error: Test '$SPECIFIC_TEST' not found"
            exit 1
        fi
    else
        TESTS_TO_RUN=("${SAN_TESTS[@]}")
    fi

    for test in "${TESTS_TO_RUN[@]}"; do
        if [ -f "$test" ]; then
            echo -n "  Testing $(basename "$test")... "
            
            # Create temp file for output capture
            local OUTPUT_FILE=$(mktemp)
            
            # Run test and capture output
            # We use 'set +e' to prevent script exit on test failure
            set +e
            php -d extension=./modules/firebird.so "$test" > "$OUTPUT_FILE" 2>&1
            EXIT_CODE=$?
set -e

            if [ $EXIT_CODE -eq 0 ]; then
                echo -e "${GREEN}PASS${NC}"
                if [ "$VERBOSE" = true ]; then
                    cat "$OUTPUT_FILE" | sed 's/^/    /'
                fi
            else
                echo -e "${RED}FAIL${NC}"
                echo "--- Output ---"
                cat "$OUTPUT_FILE"
                echo "--------------"
                FAILED=1
            fi
            rm "$OUTPUT_FILE"
        else
            log_warn "Warning: Test $test not found"
        fi
    done

    if [ $FAILED -eq 1 ]; then
        log_fail "Sanitizer tests failed"
        exit 1
    fi

    log_pass "$sanitizer_name tests completed without sanitizer errors"
}

# Main execution
case "$MODE" in
    asan)
        run_asan_build
        run_tests_with_sanitizer "AddressSanitizer"
        ;;
    ubsan)
        run_ubsan_build
        run_tests_with_sanitizer "UndefinedBehaviorSanitizer"
        ;;
    all)
        # First run ASan
        run_asan_build
        run_tests_with_sanitizer "AddressSanitizer"

        # Then run UBSan
        run_ubsan_build
        run_tests_with_sanitizer "UndefinedBehaviorSanitizer"
        ;;
esac

echo ""
log_pass "=== Sanitizer Testing Complete ==="