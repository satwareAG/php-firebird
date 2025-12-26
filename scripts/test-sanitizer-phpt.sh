#!/bin/bash
set -e

# This script is meant to be run INSIDE the docker container (php83-asan).
# Usage: ./scripts/test-sanitizer-phpt.sh [test_path]

TEST_PATH=$1

echo "Starting Sanitizer PHPT Test Run..."

# Change to extension root
cd /ext

# Clean previous builds
if [ -f Makefile ]; then
    make clean
    phpize --clean
fi

# Configure with ASan Flags
echo "Configuring build with ASan..."
phpize
export CFLAGS="-fno-omit-frame-pointer -g -fsanitize=address,leak -O0"
export CXXFLAGS="-fno-omit-frame-pointer -g -fsanitize=address,leak -O0"
export LDFLAGS="-fsanitize=address,leak"

./configure --with-firebird=/usr

# Build
echo "Building extension..."
make -j$(nproc)

# Sanitizer runtime options
export ASAN_OPTIONS="abort_on_error=1:detect_leaks=1:check_initialization_order=1:strict_init_order=1:detect_stack_use_after_return=1"
export LSAN_OPTIONS="suppressions=/ext/scripts/analysis/lsan.supp:print_suppressions=0"

# Run Tests
echo "Running tests..."
export NO_INTERACTION=1
export REPORT_EXIT_STATUS=1
export TEST_PHP_EXECUTABLE=$(which php)

if [ -n "$TEST_PATH" ]; then
    make test TESTS="$TEST_PATH"
else
    make test
fi
