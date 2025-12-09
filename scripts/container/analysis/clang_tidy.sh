#!/bin/bash
set -e

# clang-tidy validation script for C++17 modernization
echo "Running clang-tidy analysis..."

# Ensure compilation database exists
if ! [ -f compile_commands.json ]; then
    echo "Generating compilation database..."
    if command -v bear &> /dev/null; then
        bear -- make clean && bear -- make
    else
        echo "Warning: 'bear' not found. Attempting to run without compilation database (results may be less accurate)."
        # Fallback or exit depending on strictness. For now, we warn.
        # In a real CI env, we'd want to ensure compile_commands.json is generated via cmake or bear.
    fi
fi

# Run clang-tidy on extension source files (renamed from ibase_* to fbird_*)
# Using header-filter to ONLY analyze our extension's headers, not PHP system headers
clang-tidy \
    firebird.c \
    firebird_utils.cpp \
    fbird_query.c \
    fbird_query_exec.c \
    fbird_result.c \
    fbird_metadata.c \
    fbird_service.c \
    fbird_events.c \
    fbird_blobs.c \
    fbird_inspection.c \
    fbird_udf.c \
    --config-file=.clang-tidy \
    --header-filter='^\./(php_firebird|php_fbird|firebird_utils).*\.h$' \
    --format-style=file

# Check for blocking errors
if [ $? -ne 0 ]; then
    echo "❌ clang-tidy found blocking issues"
    exit 1
fi

echo "✅ clang-tidy analysis passed"
