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

# Run clang-tidy on C++ files only
# Note: firebird_utils.cpp is the main C++17 file being modernized
clang-tidy firebird_utils.cpp \
    --config-file=.clang-tidy \
    --header-filter='firebird_utils\.(h|hpp)$' \
    --format-style=file

# Check for blocking errors
if [ $? -ne 0 ]; then
    echo "❌ clang-tidy found blocking issues"
    exit 1
fi

echo "✅ clang-tidy analysis passed"
