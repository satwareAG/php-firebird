#!/bin/bash
# Generate compile_commands.json for whole-program static analysis
# This enables cppcheck and clang-tidy to see cross-file function usage
set -e

echo "Generating compile_commands.json..."

# Change to extension root directory
if [ -d /ext ]; then
    cd /ext
fi

# Clean previous builds to ensure full compilation capture
if [ -f Makefile ]; then
    make clean 2>/dev/null || true
    phpize --clean 2>/dev/null || true
fi

# Detect Firebird installation location
# Supports both apt-installed (/usr) and manual installations (/opt/firebird)
if [ -d "/opt/firebird/include" ]; then
    FB_HOME="/opt/firebird"
    FB_INCLUDE="/opt/firebird/include"
    echo "Detected manual Firebird installation at /opt/firebird"
elif [ -d "/usr/include/firebird" ]; then
    FB_HOME="/usr"
    FB_INCLUDE="/usr/include/firebird"
    echo "Detected apt-installed Firebird at /usr"
else
    FB_HOME="/usr"
    FB_INCLUDE="/usr/include/firebird"
    echo "Warning: Firebird headers not found, using default /usr"
fi

# Prepare build environment
phpize

# Configure with detected Firebird paths
CPPFLAGS="-I${FB_INCLUDE}" ./configure --with-firebird="${FB_HOME}"

# Generate compile_commands.json using bear (intercepts compilation commands)
if command -v bear &> /dev/null; then
    echo "Using bear to generate compile_commands.json..."
    bear -- make -j$(nproc)
elif command -v compiledb &> /dev/null; then
    echo "Using compiledb to generate compile_commands.json..."
    compiledb make -j$(nproc)
else
    echo "Warning: Neither 'bear' nor 'compiledb' found."
    echo "Installing compiledb via pip..."
    pip3 install compiledb --quiet
    compiledb make -j$(nproc)
fi

# Verify generation
if [ -f compile_commands.json ]; then
    ENTRIES=$(grep -c '"file"' compile_commands.json || echo 0)
    echo "✅ compile_commands.json generated with $ENTRIES compilation entries"

    # Show which files are tracked
    echo "Files in compilation database:"
    grep -oP '"file": "\K[^"]+' compile_commands.json | sort | uniq
else
    echo "❌ Failed to generate compile_commands.json"
    exit 1
fi
