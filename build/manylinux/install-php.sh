#!/bin/bash
# =============================================================================
# PHP Version Installer for Manylinux Builder
# =============================================================================
#
# Downloads and installs PHP from shivammathur/php-builder releases.
#
# Usage:
#   ./install-php.sh <version> <variant>
#   ./install-php.sh 8.4 nts
#   ./install-php.sh 8.4 zts
#
# =============================================================================

set -euo pipefail

PHP_VERSION="${1:-8.4}"
VARIANT="${2:-nts}"
INSTALL_PREFIX="/opt/php/${PHP_VERSION}-${VARIANT}"

echo "=== Installing PHP ${PHP_VERSION} (${VARIANT}) to ${INSTALL_PREFIX} ==="

# Normalize variant for URL
URL_VARIANT="${VARIANT}"
if [ "$VARIANT" = "zts" ]; then
    URL_VARIANT="zts"
else
    URL_VARIANT="nts"
fi

# shivammathur/php-builder provides pre-built PHP binaries
# URL format: https://github.com/shivammathur/php-builder/releases/download/php-x.y.z/php-x.y.z-linux-x64-nts.tar.xz
#
# Alternative: Build from source (slower but more reliable)

# Try pre-built binaries first
PREBUILT_URL="https://github.com/shivammathur/php-builder/releases/latest/download/php-${PHP_VERSION}-linux-x64-${URL_VARIANT}.tar.xz"

mkdir -p "${INSTALL_PREFIX}"

if curl -fsSL --head "${PREBUILT_URL}" | head -1 | grep -q "200\|302"; then
    echo "Downloading pre-built PHP ${PHP_VERSION}..."
    cd /tmp
    curl -fsSL -o "php-${PHP_VERSION}-${VARIANT}.tar.xz" "${PREBUILT_URL}"
    tar xf "php-${PHP_VERSION}-${VARIANT}.tar.xz" -C "${INSTALL_PREFIX}" --strip-components=1
    rm -f "php-${PHP_VERSION}-${VARIANT}.tar.xz"
else
    echo "Pre-built binary not found, building from source..."
    
    # Get latest patch version for the minor version
    PHP_FULL_VERSION=$(curl -fsSL "https://www.php.net/releases/index.php?json&version=${PHP_VERSION}" | \
        grep -oP '"version"\s*:\s*"\K[^"]+' | head -1 || echo "${PHP_VERSION}.0")
    
    if [ -z "$PHP_FULL_VERSION" ]; then
        PHP_FULL_VERSION="${PHP_VERSION}.0"
    fi
    
    echo "Building PHP ${PHP_FULL_VERSION}..."
    
    cd /tmp
    curl -fsSL -o "php-${PHP_FULL_VERSION}.tar.xz" \
        "https://www.php.net/distributions/php-${PHP_FULL_VERSION}.tar.xz"
    
    tar xf "php-${PHP_FULL_VERSION}.tar.xz"
    cd "php-${PHP_FULL_VERSION}"
    
    # Configure options
    CONFIGURE_OPTS=(
        --prefix="${INSTALL_PREFIX}"
        --with-config-file-path="${INSTALL_PREFIX}/etc"
        --enable-shared
        --disable-static
        --enable-mbstring
        --enable-bcmath
        --enable-sockets
        --enable-pcntl
        --with-openssl
        --with-curl
        --with-zlib
        --with-bz2
        --with-readline
        --with-sqlite3
        --enable-pdo
        --with-pdo-sqlite
    )
    
    if [ "$VARIANT" = "zts" ]; then
        CONFIGURE_OPTS+=(--enable-zts)
    fi
    
    ./configure "${CONFIGURE_OPTS[@]}"
    make -j"$(nproc)"
    make install
    
    # Cleanup
    cd /tmp
    rm -rf "php-${PHP_FULL_VERSION}" "php-${PHP_FULL_VERSION}.tar.xz"
fi

# Verify installation
if [ -x "${INSTALL_PREFIX}/bin/php" ]; then
    echo "PHP installed successfully:"
    "${INSTALL_PREFIX}/bin/php" -v
    
    # Check for phpize
    if [ ! -x "${INSTALL_PREFIX}/bin/phpize" ]; then
        echo "WARNING: phpize not found, extension compilation may fail"
    fi
else
    echo "ERROR: PHP installation failed!"
    exit 1
fi

echo "=== PHP ${PHP_VERSION} (${VARIANT}) installed to ${INSTALL_PREFIX} ==="
