#!/bin/bash
set -e

echo "Building PHP Firebird extension..."

# Change to extension root directory
if [ -d /ext ]; then
  cd /ext
fi

# Clean previous builds
if [ -f Makefile ]; then
    make clean
    phpize --clean
fi

# Prepare build environment
phpize

# Configure with Firebird paths
CPPFLAGS="-I/usr/include/firebird -DFBIRD_ARRAY_DEBUG" ./configure \
    --with-firebird=/usr

# Build
make -j$(nproc)

echo "Build completed. Extension is available at: $(pwd)/modules/firebird.so"
