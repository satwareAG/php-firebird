#!/bin/bash
# Cppcheck static analysis for php-firebird
# Uses .cppcheck configuration file for suppressions and settings
set -e
source "$(dirname "$(dirname "$0")")/lib/logging.sh"

log_info "Running Cppcheck static analysis..."

# Change to extension root directory
if [ -d /ext ]; then
    cd /ext
fi

# Create cache directory for faster incremental analysis
CPPCHECK_CACHE=".cppcheck-cache"
mkdir -p "$CPPCHECK_CACHE"

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

# Determine PHP include path for the current PHP version
PHP_INCLUDE_DIR=$(php-config --include-dir 2>/dev/null || echo "/usr/include/php")
PHP_API_VERSION=$(php -r 'echo PHP_VERSION_ID;' 2>/dev/null || echo "80300")

# Build include paths
INCLUDE_PATHS=(
    "-I."
    "-I${PHP_INCLUDE_DIR}"
    "-I${PHP_INCLUDE_DIR}/Zend"
    "-I${PHP_INCLUDE_DIR}/main"
    "-I${PHP_INCLUDE_DIR}/TSRM"
    "-I/usr/include/firebird"
    "-I/opt/firebird/include"
)

log_info "PHP Include Dir: $PHP_INCLUDE_DIR"
log_info "PHP API Version: $PHP_API_VERSION"

# External header false positive suppressions (absolute paths from compile_commands.json)
# - missingReturn: Zend EMPTY_SWITCH_DEFAULT_CASE() expands to __builtin_unreachable()
# - rethrowNoCurrentException: Firebird SDK throw; rethrow pattern in catch wrapper
EXTERNAL_HEADER_SUPPRESSIONS=()
PHP_INCLUDE=$(php-config --include-dir 2>/dev/null || echo "/usr/local/include/php")
EXTERNAL_HEADER_SUPPRESSIONS+=(
    "--suppress=missingReturn:${PHP_INCLUDE}/Zend/zend_compile.h"
    "--suppress=rethrowNoCurrentException:/opt/firebird/include/firebird/Interface.h"
    "--suppress=rethrowNoCurrentException:/usr/include/firebird/Interface.h"
)

# Run analysis with appropriate method
if [ -f compile_commands.json ]; then
    log_info "Using compile_commands.json for whole-program analysis..."
    cppcheck \
        --project=compile_commands.json \
        --cppcheck-build-dir="$CPPCHECK_CACHE" \
        --suppressions-list=.cppcheck-suppressions \
        "${EXTERNAL_HEADER_SUPPRESSIONS[@]}" \
        --std=c17 \
        --std=c++17 \
        --enable=warning,performance,portability \
        --inline-suppr \
        --error-exitcode=2 \
        --xml \
        --xml-version=2 \
        --output-file=cppcheck-report.xml \
        2>&1 | tee cppcheck-output.log
else
    log_warn "No compile_commands.json found, using direct file analysis..."
    log_info "Hint: Run scripts/analysis/generate_compdb.sh first for better results"

    cppcheck \
        "${SOURCE_FILES[@]}" \
        "${INCLUDE_PATHS[@]}" \
        --cppcheck-build-dir="$CPPCHECK_CACHE" \
        --suppressions-list=.cppcheck-suppressions \
        "${EXTERNAL_HEADER_SUPPRESSIONS[@]}" \
        --std=c17 \
        --std=c++17 \
        --enable=warning,performance,portability \
        --inline-suppr \
        --error-exitcode=2 \
        --xml \
        --xml-version=2 \
        --output-file=cppcheck-report.xml \
        2>&1 | tee cppcheck-output.log
fi

# Parse results
if command -v xmllint &> /dev/null; then
    ERRORS=$(xmllint --xpath 'count(//error[@severity="error"])' cppcheck-report.xml 2>/dev/null || echo 0)
    WARNINGS=$(xmllint --xpath 'count(//error[@severity="warning"])' cppcheck-report.xml 2>/dev/null || echo 0)
    PERFORMANCE=$(xmllint --xpath 'count(//error[@severity="performance"])' cppcheck-report.xml 2>/dev/null || echo 0)
    PORTABILITY=$(xmllint --xpath 'count(//error[@severity="portability"])' cppcheck-report.xml 2>/dev/null || echo 0)
else
    ERRORS=$(grep -c 'severity="error"' cppcheck-report.xml 2>/dev/null || echo 0)
    WARNINGS=$(grep -c 'severity="warning"' cppcheck-report.xml 2>/dev/null || echo 0)
    PERFORMANCE=$(grep -c 'severity="performance"' cppcheck-report.xml 2>/dev/null || echo 0)
    PORTABILITY=$(grep -c 'severity="portability"' cppcheck-report.xml 2>/dev/null || echo 0)
fi

echo ""
echo "=========================================="
log_info "Cppcheck Report Summary"
echo "=========================================="
log_info "Errors:      $ERRORS"
log_info "Warnings:    $WARNINGS"
log_info "Performance: $PERFORMANCE"
log_info "Portability: $PORTABILITY"
echo "=========================================="

# Block on errors only
if [ "$ERRORS" -gt 0 ]; then
    echo ""
    log_error "Cppcheck found $ERRORS errors"
    if command -v xmllint &> /dev/null; then
        echo ""
        log_info "Error details:"
        xmllint --xpath '//error[@severity="error"]' cppcheck-report.xml 2>/dev/null || cat cppcheck-report.xml
    else
        grep 'severity="error"' cppcheck-report.xml || true
    fi
    exit 1
fi

if [ "$WARNINGS" -gt 0 ]; then
    echo ""
    log_warn "Cppcheck found $WARNINGS warnings (review recommended)"
fi

echo ""
log_pass "Cppcheck analysis completed successfully"
log_info "   Full report: cppcheck-report.xml"
log_info "   Build cache: $CPPCHECK_CACHE/"
