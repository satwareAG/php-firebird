#!/bin/bash
set -e

echo "Building PHP Firebird extension..."

# Change to extension root directory
cd /ext

# Clean previous builds
if [ -f Makefile ]; then
    make clean
    phpize --clean
fi

# Prepare build environment
phpize

# Configure with Firebird paths
CPPFLAGS="-I/usr/include/firebird" ./configure \
    --with-interbase=/usr

# Build
make -j$(nproc)

echo "Build completed. Extension is available at: $(pwd)/modules/interbase.so"
