#!/bin/bash
# scripts/test_with_asan.sh

set -e

echo "Building with AddressSanitizer..."

# Clean build with ASan
make clean
CXXFLAGS="-fsanitize=address -fno-omit-frame-pointer -g -O1" \
    make firebird_utils.lo

# Set ASan options
export ASAN_OPTIONS="abort_on_error=1:check_initialization_order=1:strict_init_order=1"
export ASAN_SYMBOLIZER_PATH="/usr/bin/llvm-symbolizer"

# Run PHP tests with ASan
echo "Running tests with AddressSanitizer..."
php -d extension=./modules/firebird.so -m | grep "firebird"

# Run specific test that exercises modernized functions
php -d extension=./modules/firebird.so tests/fbclient_vers_001.phpt

echo "✅ AddressSanitizer testing completed"
