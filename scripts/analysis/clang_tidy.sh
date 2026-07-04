#!/bin/bash
# clang-tidy static analysis for php-firebird
# Uses .clang-tidy configuration for C++17 modernization checks
set -e
source "$(dirname "$(dirname "$0")")/lib/logging.sh"

log_info "Running clang-tidy analysis..."

# Change to extension root directory
if [ -d /ext ]; then
    cd /ext
fi

# All C/C++ source files in the extension
SOURCE_FILES=(
    firebird.c
    firebird_utils.cpp
    fbird_blobs.c
    fbird_datetime.c
    fbird_events.c
    fbird_inspection.c
    fbird_metadata.c
    fbird_query_array.c
    fbird_query_bind.c
    fbird_query_exec.c
    fbird_query_prepare.c
    fbird_result.c
    fbird_service.c
)

# Ensure compilation database exists for accurate analysis
if ! [ -f compile_commands.json ]; then
    log_warn "No compile_commands.json found."
    log_info "Attempting to generate with bear..."

    if command -v bear &> /dev/null; then
        # Clean and rebuild with bear
        if [ -f Makefile ]; then
            make clean 2>/dev/null || true
        else
            phpize
            CPPFLAGS="-I/usr/include/firebird" ./configure --with-firebird=/usr
        fi
        bear -- make -j$(nproc)
    else
        log_warn "'bear' not found. Running without compilation database."
        log_warn "Results may be less accurate. Consider installing bear:"
        log_info "  apt-get install bear"

        # Build if not already built
        if ! [ -f modules/firebird.so ]; then
            phpize
            CPPFLAGS="-I/usr/include/firebird" ./configure --with-firebird=/usr
            make -j$(nproc)
        fi
    fi
fi

# Determine clang-tidy arguments
CLANG_TIDY_ARGS=(
    "--config-file=.clang-tidy"
    "--header-filter=^\./(php_firebird|php_fbird|firebird_utils).*\.h$"
)

# Add compilation database if available
if [ -f compile_commands.json ]; then
    CLANG_TIDY_ARGS+=("-p" ".")
    log_info "Using compile_commands.json for analysis"
else
    # Provide manual include paths if no compile_commands.json
    PHP_INCLUDE_DIR=$(php-config --include-dir 2>/dev/null || echo "/usr/include/php")

    # Detect Firebird include path (supports both apt-installed and manual installations)
    if [ -d "/opt/firebird/include" ]; then
        FB_INCLUDE_DIR="/opt/firebird/include"
    elif [ -d "/usr/include/firebird" ]; then
        FB_INCLUDE_DIR="/usr/include/firebird"
    else
        FB_INCLUDE_DIR="/usr/include/firebird"
    fi

    CLANG_TIDY_ARGS+=(
        "--"
        "-I."
        "-I${PHP_INCLUDE_DIR}"
        "-I${PHP_INCLUDE_DIR}/Zend"
        "-I${PHP_INCLUDE_DIR}/main"
        "-I${PHP_INCLUDE_DIR}/TSRM"
        "-I${FB_INCLUDE_DIR}"
        "-std=c++17"
        "-DHAVE_CONFIG_H"
    )
    log_info "Using manual include paths (compile_commands.json recommended)"
    log_info "Firebird include: ${FB_INCLUDE_DIR}"
fi

echo ""
log_info "Analyzing ${#SOURCE_FILES[@]} source files..."

# Track results
FAILED=0
PASSED=0

for file in "${SOURCE_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo -n "  Checking $file... "
        if clang-tidy "$file" "${CLANG_TIDY_ARGS[@]}" 2>&1 | tee -a clang-tidy-output.log | grep -q "error:"; then
            echo "❌"
            ((FAILED+=1))
        else
            echo "✓"
            ((PASSED+=1))
        fi
    else
        log_warn "Skipping $file (not found)"
    fi
done

echo ""
echo "=========================================="
log_info "clang-tidy Report Summary"
echo "=========================================="
log_info "Passed: $PASSED"
log_info "Failed: $FAILED"
log_info "Total:  ${#SOURCE_FILES[@]}"
echo "=========================================="

# Check for blocking errors (WarningsAsErrors in .clang-tidy)
if [ $FAILED -gt 0 ]; then
    echo ""
    log_error "clang-tidy found blocking issues in $FAILED files"
    log_info "   See clang-tidy-output.log for details"
    exit 1
fi

echo ""
log_pass "clang-tidy analysis passed"
log_info "   Full output: clang-tidy-output.log"
