#!/bin/bash
set -e

echo "Building PHP Firebird extension..."

# Change to extension root directory
if [ -d /ext ]; then
  cd /ext
fi

# Clean previous builds thoroughly
# Remove dependency files first - they contain absolute paths to PHP headers
# that differ between PHP versions and cause "No rule to make target" errors
find . -name '*.dep' -delete 2>/dev/null || true
find . -name '*.lo' -delete 2>/dev/null || true
rm -rf .libs pdo_fbird/.libs 2>/dev/null || true

if [ -f Makefile ]; then
    make clean 2>/dev/null || true
    phpize --clean 2>/dev/null || true
fi

# Explicitly remove autoconf/configure artifacts in case phpize --clean failed.
# Stale config.h from a different PHP version causes silent build mismatches.
rm -f configure config.h config.h.in config.log config.status config.nice \
     Makefile Makefile.fragments Makefile.global Makefile.objects \
     build/shtool config.cache libtool 2>/dev/null || true

# Clean any standalone pdo_fbird build artifacts — pdo_fbird is compiled
# as part of the unified firebird.so via config.m4.  A leftover
# pdo_fbird/config.h from a standalone build defines COMPILE_DL_PDO_FBIRD
# which causes duplicate get_module symbols.
if [ -f pdo_fbird/Makefile ]; then
    (cd pdo_fbird && make clean 2>/dev/null || true && phpize --clean 2>/dev/null || true)
fi
rm -f pdo_fbird/config.h pdo_fbird/config.h.in~ pdo_fbird/config.log \
     pdo_fbird/config.status pdo_fbird/config.nice 2>/dev/null || true

# Prepare build environment
phpize

# Auto-detect Firebird paths
# Priority: FIREBIRD_HOME env > fb_config on PATH (system package) > /opt/firebird > /usr
#
# When fb_config is available (system package like Arch libfbclient, Debian
# firebird-dev, Fedora), pass --with-firebird=yes (no path) and let
# config.m4 handle cflags/libs/version check via fb_config directly.
# This avoids fragile sed/dirname parsing and correctly handles multiarch
# library paths (e.g. /usr/lib/x86_64-linux-gnu on Debian).
if [ -n "$FIREBIRD_HOME" ] && [ -d "$FIREBIRD_HOME" ]; then
    FIREBIRD_PATH="$FIREBIRD_HOME"
    FIREBIRD_INCLUDE="$FIREBIRD_HOME/include"
    CONFIGURE_ARGS="--with-firebird=$FIREBIRD_PATH"
    echo "Using FIREBIRD_HOME: $FIREBIRD_PATH"
elif command -v fb_config >/dev/null 2>&1; then
    # System-installed libfbclient — let config.m4 query fb_config for
    # cflags, libs, and Firebird 3.0+ version check.
    FIREBIRD_PATH="$(dirname "$(dirname "$(command -v fb_config)")")"
    FIREBIRD_INCLUDE="$(fb_config --cflags)"
    CONFIGURE_ARGS="--with-firebird=yes"
    echo "Using system fb_config ($(fb_config --version)): $FIREBIRD_PATH"
elif [ -d "/opt/firebird" ]; then
    FIREBIRD_PATH="/opt/firebird"
    FIREBIRD_INCLUDE="/opt/firebird/include"
    CONFIGURE_ARGS="--with-firebird=$FIREBIRD_PATH"
    echo "Auto-detected Firebird at /opt/firebird"
else
    FIREBIRD_PATH="/usr"
    FIREBIRD_INCLUDE="/usr/include/firebird"
    CONFIGURE_ARGS="--with-firebird=$FIREBIRD_PATH"
    echo "Using system Firebird at /usr"
fi

# Configure with Firebird paths
# When using --with-firebird=yes (fb_config mode), config.m4 injects the
# correct -I flags from fb_config --cflags, so CPPFLAGS is not needed.
# For explicit path mode, CPPFLAGS ensures headers are found.
if [ "$CONFIGURE_ARGS" = "--with-firebird=yes" ]; then
    ./configure $CONFIGURE_ARGS
else
    CPPFLAGS="-I$FIREBIRD_INCLUDE" ./configure $CONFIGURE_ARGS
fi

# Build
make -j$(nproc)

echo "Build completed. Extension is available at: $(pwd)/modules/firebird.so"
echo "Firebird client library: $FIREBIRD_PATH"
echo "Note: pdo_fbird PDO driver is integrated into firebird.so (no separate build needed)"