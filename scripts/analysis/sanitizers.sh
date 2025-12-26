#!/bin/bash
# scripts/analysis/sanitizers.sh
# Comprehensive sanitizer testing: AddressSanitizer + UndefinedBehaviorSanitizer + LeakSanitizer
# Usage: ./sanitizers.sh [mode]
#   mode: 'asan' (default), 'ubsan', 'all'

set -e

MODE=${1:-all}
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXT_DIR="${SCRIPT_DIR}/../.."

cd "$EXT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}=== Sanitizer Testing Suite ===${NC}"
echo "Mode: $MODE"

# Common sanitizer flags
COMMON_FLAGS="-fno-omit-frame-pointer -g -O1"
ASAN_FLAGS="-fsanitize=address,leak"
UBSAN_FLAGS="-fsanitize=undefined,integer,nullability -fno-sanitize-recover=all"

run_asan_build() {
    echo -e "\n${BLUE}>> Building with AddressSanitizer + LeakSanitizer...${NC}"

    # Clean previous build
    if [ -f Makefile ]; then
        make clean 2>/dev/null || true
        phpize --clean 2>/dev/null || true
    fi

    phpize

    # Configure with ASan flags
    CFLAGS="$COMMON_FLAGS $ASAN_FLAGS" \
    CXXFLAGS="$COMMON_FLAGS $ASAN_FLAGS" \
    LDFLAGS="$ASAN_FLAGS" \
    ./configure --with-firebird=/usr

    make -j"$(nproc)"

    echo -e "${GREEN}✓ ASan build complete${NC}"
}

run_ubsan_build() {
    echo -e "\n${BLUE}>> Building with UndefinedBehaviorSanitizer...${NC}"

    # Clean previous build
    if [ -f Makefile ]; then
        make clean 2>/dev/null || true
        phpize --clean 2>/dev/null || true
    fi

    phpize

    # Configure with UBSan flags
    CFLAGS="$COMMON_FLAGS $UBSAN_FLAGS" \
    CXXFLAGS="$COMMON_FLAGS $UBSAN_FLAGS" \
    LDFLAGS="$UBSAN_FLAGS" \
    ./configure --with-firebird=/usr

    make -j"$(nproc)"

    echo -e "${GREEN}✓ UBSan build complete${NC}"
}

run_tests_with_sanitizer() {
    local sanitizer_name=$1

    echo -e "\n${BLUE}>> Running tests with $sanitizer_name...${NC}"

    # Sanitizer runtime options
    export ASAN_OPTIONS="abort_on_error=1:detect_leaks=1:check_initialization_order=1:strict_init_order=1:detect_stack_use_after_return=1"
    export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"
    export LSAN_OPTIONS="suppressions=/ext/scripts/analysis/lsan.supp:print_suppressions=0"

    # Find llvm-symbolizer for better stack traces
    if [ -f /usr/bin/llvm-symbolizer ]; then
        export ASAN_SYMBOLIZER_PATH="/usr/bin/llvm-symbolizer"
    elif [ -f /usr/lib/llvm-14/bin/llvm-symbolizer ]; then
        export ASAN_SYMBOLIZER_PATH="/usr/lib/llvm-14/bin/llvm-symbolizer"
    fi

    # Verify extension loads
    echo "Verifying extension loads..."
    if ! php -d extension=./modules/firebird.so -m 2>&1 | grep -q "firebird"; then
        echo -e "${RED}✗ Extension failed to load${NC}"
        exit 1
    fi
    echo -e "${GREEN}✓ Extension loaded successfully${NC}"

    # Run dedicated sanitizer tests
    echo "Running dedicated sanitizer tests..."
    local SAN_TESTS=(
        "tests/sanitizer/asan_basic.php"
        "tests/sanitizer/blob_operations.php"
        "tests/sanitizer/transaction_stress.php"
    )

    local FAILED=0
    for test in "${SAN_TESTS[@]}"; do
        if [ -f "$test" ]; then
            echo -n "  Testing $test... "
            # Capture output and exit code
            OUTPUT=$(php -d extension=./modules/firebird.so "$test" 2>&1)
            EXIT_CODE=$?
            
            if [ $EXIT_CODE -eq 0 ]; then
                echo -e "${GREEN}PASS${NC}"
            else
                echo -e "${RED}FAIL${NC}"
                echo "$OUTPUT"
                FAILED=1
            fi
        else
            echo -e "${YELLOW}Warning: Test $test not found${NC}"
        fi
    done

    if [ $FAILED -eq 1 ]; then
        echo -e "${RED}✗ Sanitizer tests failed${NC}"
        exit 1
    fi

    echo -e "${GREEN}✓ $sanitizer_name tests completed without sanitizer errors${NC}"
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
    *)
        echo "Usage: $0 [asan|ubsan|all]"
        exit 1
        ;;
esac

echo -e "\n${GREEN}=== Sanitizer Testing Complete ===${NC}"
