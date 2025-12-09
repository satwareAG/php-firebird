#!/bin/bash
set -e

echo "Running Cppcheck static analysis..."

# Create cppcheck project file if needed
if ! [ -f php-firebird.cppcheck ]; then
    # Generate a basic project file on the fly if it doesn't exist,
    # utilizing compile_commands.json if available for better accuracy.
    if [ -f compile_commands.json ]; then
        cppcheck --project=compile_commands.json \
                 --std=c++17 \
                 --enable=all \
                 --inconclusive \
                 --xml \
                 --xml-version=2 \
                 --output-file=cppcheck-report.xml
    else
        # Fallback to processing the file directly with broad checks
        cppcheck firebird_utils.cpp firebird.c fbird_query_exec.c fbird_result.c fbird_metadata.c fbird_service.c fbird_events.c fbird_blobs.c \
                 --std=c++17 \
                 --enable=all \
                 --inconclusive \
                 --xml \
                 --xml-version=2 \
                 --output-file=cppcheck-report.xml \
                 -I. -I/usr/include/php -I/usr/include/firebird
    fi
else
    # Use the existing configuration file
    cppcheck --project=php-firebird.cppcheck \
             --xml \
             --xml-version=2 \
             --output-file=cppcheck-report.xml
fi

# Parse results
# Note: xmllint might not be installed in all environments; simple grep fallback
if command -v xmllint &> /dev/null; then
    ERRORS=$(xmllint --xpath 'count(//error[@severity="error"])' cppcheck-report.xml 2>/dev/null || echo 0)
    WARNINGS=$(xmllint --xpath 'count(//error[@severity="warning"])' cppcheck-report.xml 2>/dev/null || echo 0)
else
    ERRORS=$(grep -c 'severity="error"' cppcheck-report.xml || echo 0)
    WARNINGS=$(grep -c 'severity="warning"' cppcheck-report.xml || echo 0)
fi

echo "Cppcheck results: $ERRORS errors, $WARNINGS warnings"

# Block on errors, allow warnings
if [ "$ERRORS" -gt 0 ]; then
    echo "❌ Cppcheck found $ERRORS errors"
    # Show errors content if xmllint available
    if command -v xmllint &> /dev/null; then
        # Try to print errors, but also cat the file if that fails or returns empty
        xmllint --xpath '//error[@severity="error"]' cppcheck-report.xml || cat cppcheck-report.xml
    else
        cat cppcheck-report.xml
    fi
    exit 1
fi

if [ "$WARNINGS" -gt 0 ]; then
    echo "⚠️ Cppcheck found $WARNINGS warnings (review recommended)"
fi

echo "✅ Cppcheck analysis completed"
