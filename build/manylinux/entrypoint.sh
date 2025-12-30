#!/bin/bash
# =============================================================================
# PHP Firebird Extension Builder - Entrypoint
# =============================================================================
#
# Builds the PHP extension for a specific PHP version and variant.
#
# Usage:
#   docker run --rm -v $(pwd):/src php-firebird-builder 8.4 nts x86_64
#
# =============================================================================

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}>>>${NC} $*"
}

log_error() {
    echo -e "${RED}ERROR:${NC} $*" >&2
}

# Parse arguments
PHP_VERSION="${1:-8.4}"
VARIANT="${2:-nts}"
ARCH="${3:-x86_64}"

log_info "=== PHP Firebird Extension Builder ==="
log_info "PHP Version: ${PHP_VERSION}"
log_info "Variant: ${VARIANT}"
log_info "Architecture: ${ARCH}"
echo ""

# Select PHP installation
PHP_PREFIX="/opt/php/${PHP_VERSION}-${VARIANT}"

if [ ! -d "${PHP_PREFIX}" ]; then
    log_error "PHP ${PHP_VERSION} (${VARIANT}) not installed at ${PHP_PREFIX}"
    log_error "Available PHP installations:"
    ls -1 /opt/php/ 2>/dev/null || echo "  (none)"
    exit 1
fi

# Set PHP environment
export PATH="${PHP_PREFIX}/bin:${PATH}"
export PHP_CONFIG="${PHP_PREFIX}/bin/php-config"

log_info "Using PHP from: ${PHP_PREFIX}"
php -v

# Check source directory
if [ ! -f "/src/php_firebird.h" ]; then
    log_error "Source directory /src does not contain php_firebird.h"
    log_error "Mount the project root: docker run -v \$(pwd):/src ..."
    exit 1
fi

cd /src

# Clean previous build artifacts
log_info "Cleaning previous build..."
phpize --clean 2>/dev/null || true
rm -rf modules/ autom4te.cache/ 2>/dev/null || true

# Create dist directory
mkdir -p dist

# Set environment for build
export FB_ROOT="/opt/firebird"
export EXT_VERSION="${EXT_VERSION:-$(grep '#define PHP_FIREBIRD_VERSION' php_firebird.h | sed 's/.*"\([^"]*\)".*/\1/' || echo '7.0.0')}"
export PHP_PREFIX

# Run the build script
log_info "Running build-precompiled.sh..."
./scripts/build-precompiled.sh "${PHP_VERSION}" "${VARIANT}" "${ARCH}" --verbose

# Verify output
PHP_VER_SHORT="${PHP_VERSION//.}"
DIST_TARBALL="dist/php-firebird-${EXT_VERSION}-php${PHP_VER_SHORT}-${VARIANT}-linux-${ARCH}.tar.gz"

if [ -f "${DIST_TARBALL}" ]; then
    log_info "Build successful!"
    ls -lh "${DIST_TARBALL}"
    
    # Run verification if available
    if [ -x "scripts/verify-bundle.sh" ]; then
        log_info "Running verification..."
        ./scripts/verify-bundle.sh "${DIST_TARBALL}" --verbose || true
    fi
else
    log_error "Build failed - output tarball not found: ${DIST_TARBALL}"
    exit 1
fi

echo ""
log_info "=== Build Complete ==="
