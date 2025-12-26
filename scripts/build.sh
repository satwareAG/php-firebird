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

# Auto-detect Firebird paths
# FIREBIRD_HOME environment variable takes precedence (set in FB3/FB5 containers)
# Otherwise defaults to /usr (apt-installed firebird-dev package)
if [ -n "$FIREBIRD_HOME" ] && [ -d "$FIREBIRD_HOME" ]; then
    FIREBIRD_PATH="$FIREBIRD_HOME"
    FIREBIRD_INCLUDE="$FIREBIRD_HOME/include"
    echo "Using FIREBIRD_HOME: $FIREBIRD_PATH"
elif [ -d "/opt/firebird" ]; then
    FIREBIRD_PATH="/opt/firebird"
    FIREBIRD_INCLUDE="/opt/firebird/include"
    echo "Auto-detected Firebird at /opt/firebird"
else
    FIREBIRD_PATH="/usr"
    FIREBIRD_INCLUDE="/usr/include/firebird"
    echo "Using system Firebird at /usr"
fi

# Configure with Firebird paths
CPPFLAGS="-I$FIREBIRD_INCLUDE" ./configure \
    --with-firebird="$FIREBIRD_PATH"

# Build
make -j$(nproc)

echo "Build completed. Extension is available at: $(pwd)/modules/firebird.so"
echo "Firebird client library: $FIREBIRD_PATH"
