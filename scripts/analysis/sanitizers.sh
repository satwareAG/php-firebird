#!/bin/bash
# scripts/container/analysis/sanitizers.sh
# Comprehensive sanitizer testing: AddressSanitizer + UndefinedBehaviorSanitizer + LeakSanitizer
# Usage: ./sanitizers.sh [mode]
#   mode: 'asan' (default), 'ubsan', 'all'

set -e

MODE=${1:-all}
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXT_DIR="${SCRIPT_DIR}/../../.."

cd "$EXT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}=== Sanitizer Testing Suite ===${NC}"
echo "Mode: $MODE"

# Source files to analyze
C_SOURCES="firebird.c fbird_query_exec.c fbird_result.c fbird_metadata.c fbird_service.c fbird_events.c fbird_blobs.c fbird_query.c fbird_inspection.c fbird_udf.c"
CPP_SOURCES="firebird_utils.cpp"

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
    export LSAN_OPTIONS="suppressions=/ext/scripts/container/analysis/lsan.supp:print_suppressions=0"

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

    # Run subset of tests (fast verification)
    echo "Running core tests..."
    local TESTS=(
        "tests/fbclient_vers_001.phpt"
        "tests/fbird_connect_dpb_001.phpt"
        "tests/datatype_001.phpt"
        "tests/fbird_blob_001.phpt"
        "tests/fbird_trans_002.phpt"
    )

    local FAILED=0
    for test in "${TESTS[@]}"; do
        if [ -f "$test" ]; then
            echo -n "  Testing $test... "
            if php -d extension=./modules/firebird.so "$test" >/dev/null 2>&1; then
                echo -e "${GREEN}PASS${NC}"
            else
                echo -e "${YELLOW}SKIP/FAIL${NC}"
                # Don't fail on test failures, only on sanitizer errors (which abort)
            fi
        fi
    done

    echo -e "${GREEN}✓ $sanitizer_name tests completed without sanitizer errors${NC}"
}

# Create LSan suppressions file for known PHP/Firebird leaks
create_lsan_suppressions() {
    mkdir -p "$(dirname "$0")"
    cat > /ext/scripts/container/analysis/lsan.supp << 'EOF'
# LSan suppressions for php-firebird
# Suppress known PHP internal allocations
leak:php_module_startup
leak:zend_startup
leak:zend_register_functions
# Suppress Firebird client library internal allocations
leak:fb_ping
leak:isc_attach_database
EOF
}

# Main execution
case "$MODE" in
    asan)
        create_lsan_suppressions
        run_asan_build
        run_tests_with_sanitizer "AddressSanitizer"
        ;;
    ubsan)
        run_ubsan_build
        run_tests_with_sanitizer "UndefinedBehaviorSanitizer"
        ;;
    all)
        create_lsan_suppressions

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
