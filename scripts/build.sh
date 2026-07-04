#!/bin/bash
set -e

# Change to extension root directory
if [ -d /ext ]; then
  cd /ext
fi
source "$(dirname "$0")/lib/logging.sh"
source "$(dirname "$0")/lib/detect-firebird.sh"

log_info "Building PHP Firebird extension..."

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

# Prepare build environment
phpize

# Auto-detect Firebird paths using shared helper
detect_firebird

# Configure with Firebird paths
# When using --with-firebird=yes (fb_config mode), config.m4 injects the
# correct -I flags from fb_config --cflags, so CPPFLAGS is not needed.
# For explicit path mode, CPPFLAGS ensures headers are found.
if [ "$CONFIGURE_ARGS" = "--with-firebird=yes" ]; then
    ./configure $CONFIGURE_ARGS ${FBIRD_CONFIGURE_EXTRA:-}
else
    CPPFLAGS="-I$FIREBIRD_INCLUDE" ./configure $CONFIGURE_ARGS ${FBIRD_CONFIGURE_EXTRA:-}
fi

# Build
make -j$(nproc)

log_info "Build completed. Extension is available at: $(pwd)/modules/firebird.so"
log_info "Firebird client library: $FIREBIRD_PATH"

# Build pdo_fbird as separate extension (depends on firebird.so symbols)
if [ -d pdo_fbird ] && [ -f pdo_fbird/config.m4 ]; then
    log_info "Building pdo_fbird as separate extension..."
    (cd pdo_fbird && \
        phpize && \
        ./configure --with-pdo-fbird ${FBIRD_CONFIGURE_EXTRA:-} && \
        make -j$(nproc))
    log_info "pdo_fbird extension: $(pwd)/pdo_fbird/modules/pdo_fbird.so"
    log_info "Note: pdo_fbird PDO driver is a separate extension (load after firebird)"
else
    log_info "Note: pdo_fbird not built (pdo_fbird/config.m4 not found)"
fi