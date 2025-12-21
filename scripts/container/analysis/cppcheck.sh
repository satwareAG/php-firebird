#!/bin/bash
# Cppcheck static analysis for php-firebird
# Uses .cppcheck configuration file for suppressions and settings
set -e

echo "Running Cppcheck static analysis..."

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
    fbird_query.c
    fbird_query_array.c
    fbird_query_bind.c
    fbird_query_exec.c
    fbird_query_prepare.c
    fbird_result.c
    fbird_service.c
    fbird_udf.c
)

# Determine PHP include path for the current PHP version
PHP_INCLUDE_DIR=$(php-config --include-dir 2>/dev/null || echo "/usr/include/php")
PHP_API_VERSION=$(php -r 'echo PHP_API_VERSION;' 2>/dev/null || echo "20240924")

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

echo "PHP Include Dir: $PHP_INCLUDE_DIR"
echo "PHP API Version: $PHP_API_VERSION"

# Run analysis with appropriate method
if [ -f compile_commands.json ]; then
    echo "Using compile_commands.json for whole-program analysis..."
    cppcheck \
        --project=compile_commands.json \
        --cppcheck-build-dir="$CPPCHECK_CACHE" \
        --suppressions-list=.cppcheck-suppressions \
        --std=c17 \
        --std=c++17 \
        --enable=warning,performance,portability \
        --suppress=missingIncludeSystem \
        --suppress=staticFunction \
        --suppress=unusedFunction \
        --inline-suppr \
        --error-exitcode=2 \
        --xml \
        --xml-version=2 \
        --output-file=cppcheck-report.xml \
        2>&1 | tee cppcheck-output.log
else
    echo "No compile_commands.json found, using direct file analysis..."
    echo "Hint: Run scripts/container/analysis/generate_compdb.sh first for better results"

    cppcheck \
        "${SOURCE_FILES[@]}" \
        "${INCLUDE_PATHS[@]}" \
        --cppcheck-build-dir="$CPPCHECK_CACHE" \
        --suppressions-list=.cppcheck-suppressions \
        --std=c17 \
        --std=c++17 \
        --enable=warning,performance,portability \
        --suppress=missingIncludeSystem \
        --suppress=staticFunction \
        --suppress=unusedFunction \
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
echo "Cppcheck Report Summary"
echo "=========================================="
echo "Errors:      $ERRORS"
echo "Warnings:    $WARNINGS"
echo "Performance: $PERFORMANCE"
echo "Portability: $PORTABILITY"
echo "=========================================="

# Block on errors only
if [ "$ERRORS" -gt 0 ]; then
    echo ""
    echo "❌ Cppcheck found $ERRORS errors"
    if command -v xmllint &> /dev/null; then
        echo ""
        echo "Error details:"
        xmllint --xpath '//error[@severity="error"]' cppcheck-report.xml 2>/dev/null || cat cppcheck-report.xml
    else
        grep 'severity="error"' cppcheck-report.xml || true
    fi
    exit 1
fi

if [ "$WARNINGS" -gt 0 ]; then
    echo ""
    echo "⚠️  Cppcheck found $WARNINGS warnings (review recommended)"
fi

echo ""
echo "✅ Cppcheck analysis completed successfully"
echo "   Full report: cppcheck-report.xml"
echo "   Build cache: $CPPCHECK_CACHE/"
