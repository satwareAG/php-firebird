# syntax=docker/dockerfile:1.4
# =============================================================================
# PHP Firebird Extension - manylinux_2_28 Builder
# =============================================================================
#
# Based on manylinux_2_28 (AlmaLinux 8, glibc 2.28) for maximum Linux
# distribution compatibility.
#
# Compatible distributions (glibc 2.28+):
#   - Ubuntu 18.10+, 20.04 LTS+, 22.04 LTS+, 24.04 LTS+
#   - Debian 10 (Buster)+
#   - RHEL/CentOS/AlmaLinux/Rocky Linux 8+
#   - Fedora 29+
#   - openSUSE Leap 15.1+
#
# Usage:
#   docker build -t php-firebird-manylinux -f docker/manylinux/Dockerfile .
#   docker run --rm -v $(pwd)/dist:/dist php-firebird-manylinux
#
# =============================================================================

FROM quay.io/pypa/manylinux_2_28_x86_64

LABEL maintainer="satware AG <info@satware.com>"
LABEL description="PHP Firebird extension builder on manylinux_2_28 (glibc 2.28)"

# =============================================================================
# Configuration
# =============================================================================

# Firebird version and build number
ARG FB_VERSION=5.0.3
ARG FB_BUILD=1683-0

# PHP versions to build (PHP 8.2+ only)
ARG PHP_VERSIONS="8.2.32 8.3.32 8.4.23 8.5.8"

# Extension branch
ARG EXT_BRANCH=feature/dev-7.0.0

# Installation paths
ENV FB_ROOT=/opt/firebird
ENV PHP_BASE=/opt/php

# =============================================================================
# Install Build Dependencies
# =============================================================================

# manylinux_2_28 is based on AlmaLinux 8, use dnf
RUN dnf install -y \
    autoconf \
    automake \
    bison \
    make \
    gcc \
    gcc-c++ \
    git \
    wget \
    curl \
    patchelf \
    libtool \
    libxml2-devel \
    sqlite-devel \
    bzip2-devel \
    libcurl-devel \
    libpng-devel \
    libjpeg-devel \
    freetype-devel \
    oniguruma-devel \
    libzip-devel \
    openssl-devel \
    ncurses-devel \
    readline-devel \
    libtommath \
    libtomcrypt \
    libtomcrypt-devel \
    re2 \
    re2-devel \
    libicu \
    libicu-devel \
    && dnf clean all

# =============================================================================
# Build re2c from Source (AlmaLinux 8's re2c is too old for PHP 8.4+)
# =============================================================================

# PHP 8.4+ requires re2c 1.0.3+, AlmaLinux 8 has 0.14.3
RUN set -eux && \
    cd /tmp && \
    curl -fL -o re2c.tar.xz https://github.com/skvadrik/re2c/releases/download/3.1/re2c-3.1.tar.xz && \
    tar xf re2c.tar.xz && \
    cd re2c-3.1 && \
    ./configure --prefix=/usr/local && \
    make -j$(nproc) && \
    make install && \
    rm -rf /tmp/re2c* && \
    /usr/local/bin/re2c --version

# =============================================================================
# Download and Extract Firebird SDK
# =============================================================================

RUN set -eux && \
    mkdir -p ${FB_ROOT} && \
    cd /tmp && \
    echo "Resolving latest Firebird 5.x release..." && \
    TAG=$(curl -s https://api.github.com/repos/FirebirdSQL/firebird/releases | jq -r '.[] | select(.tag_name | test("^v5\\.")) | .tag_name' | head -n 1) && \
    ASSET_URL=$(curl -s https://api.github.com/repos/FirebirdSQL/firebird/releases/tags/${TAG} | jq -r '.assets[] | select(.name | test("linux-x64\\.tar\\.gz$") and (test("debug") | not)) | .browser_download_url' | head -n 1) && \
    ASSET_NAME=$(basename "${ASSET_URL}") && \
    FB_VERSION=${TAG#v} && \
    EXTRACT_DIR="${ASSET_NAME%.tar.gz}" && \
    echo "Downloading Firebird ${FB_VERSION} from ${ASSET_URL}..." && \
    curl -fL -o firebird.tar.gz "${ASSET_URL}" && \
    echo "Extracting outer tarball..." && \
    tar xzf firebird.tar.gz && \
    cd "${EXTRACT_DIR}" && \
    echo "Extracting buildroot.tar.gz..." && \
    tar xzf buildroot.tar.gz && \
    echo "Installing Firebird SDK to ${FB_ROOT}..." && \
    mkdir -p ${FB_ROOT}/{lib,include} && \
    cp -a opt/firebird/lib/* ${FB_ROOT}/lib/ && \
    cp -a opt/firebird/include/* ${FB_ROOT}/include/ && \
    echo "Creating symlinks..." && \
    cd ${FB_ROOT}/lib && \
    # Ensure we have the correct symlink chain
    ls -la && \
    # Create .so symlink if it doesn't exist
    if [ ! -e libfbclient.so ]; then \
        ln -sf libfbclient.so.2 libfbclient.so; \
    fi && \
    echo "Verifying Firebird SDK installation..." && \
    ls -la ${FB_ROOT}/lib/ && \
    ls -la ${FB_ROOT}/include/ && \
    test -f ${FB_ROOT}/lib/libfbclient.so && \
    test -f ${FB_ROOT}/include/ibase.h && \
    echo "Firebird SDK installed successfully" && \
    rm -rf /tmp/firebird* /tmp/Firebird*

# =============================================================================
# Download and Build PHP Versions
# =============================================================================

# Clone PHP source for each version
RUN set -eux && \
    mkdir -p /usr/src && \
    for php in ${PHP_VERSIONS}; do \
        echo "Cloning PHP ${php}..." && \
        git clone --depth 1 --branch php-${php} \
            https://github.com/php/php-src.git /usr/src/php-${php}; \
    done

# Build each PHP version (NTS only for bundle building)
RUN set -eux && \
    for php in ${PHP_VERSIONS}; do \
        php_short=${php%.*} && \
        echo "Building PHP ${php} (NTS)..." && \
        cd /usr/src/php-${php} && \
        ./buildconf --force && \
        ./configure \
            --prefix=${PHP_BASE}-${php_short} \
            --disable-all \
            --disable-cgi \
            --enable-cli \
            --with-openssl \
            --enable-mbstring \
            --build=x86_64-linux-gnu \
            --host=x86_64-linux-gnu && \
        make -j$(nproc) && \
        make install && \
        echo "PHP ${php_short} installed to ${PHP_BASE}-${php_short}" && \
        ${PHP_BASE}-${php_short}/bin/php -v; \
    done

# =============================================================================
# Clone php-firebird Extension
# =============================================================================

RUN set -eux && \
    cd /usr/src && \
    git clone --depth 1 --branch ${EXT_BRANCH} \
        https://github.com/satwareAG/php-firebird.git

# =============================================================================
# Build Script
# =============================================================================

# Create the build script that will be run inside the container
RUN <<'SCRIPT' cat > /usr/local/bin/build-bundles.sh
#!/bin/bash
set -euo pipefail

# Configuration
FB_ROOT="${FB_ROOT:-/opt/firebird}"
PHP_BASE="${PHP_BASE:-/opt/php}"
SRC_DIR="/usr/src/php-firebird"
OUTPUT_DIR="${OUTPUT_DIR:-/dist}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}>>>${NC} $*"; }
log_warn() { echo -e "${YELLOW}WARNING:${NC} $*"; }
log_error() { echo -e "${RED}ERROR:${NC} $*" >&2; }

# Get extension version
cd "$SRC_DIR"
EXT_VERSION=$(grep -E '#define PHP_FIREBIRD_VERSION' php_firebird.h 2>/dev/null | \
    sed 's/.*"\([^"]*\)".*/\1/' | head -1 || echo "7.0.0")

log_info "Building php-firebird ${EXT_VERSION} bundles..."
log_info "Output directory: ${OUTPUT_DIR}"

mkdir -p "${OUTPUT_DIR}"

# Build for each PHP version
for php_dir in ${PHP_BASE}-*; do
    if [ ! -d "$php_dir" ]; then
        continue
    fi
    
    php_short=$(basename "$php_dir" | sed 's/php-//')
    log_info "Building for PHP ${php_short}..."
    
    cd "$SRC_DIR"
    
    # Clean previous build
    make clean 2>/dev/null || true
    ${php_dir}/bin/phpize --clean 2>/dev/null || true
    
    # Configure
    log_info "  Running phpize..."
    ${php_dir}/bin/phpize
    
    log_info "  Configuring..."
    ./configure \
        --with-php-config=${php_dir}/bin/php-config \
        --with-firebird=${FB_ROOT} \
        || { cat config.log; exit 1; }
    
    # Build
    log_info "  Compiling..."
    make -j$(nproc)
    
    # Verify build
    if [ ! -f modules/firebird.so ]; then
        log_error "Build failed for PHP ${php_short}"
        continue
    fi
    
    # Bundle with libraries
    log_info "  Creating bundle..."
    
    BUNDLE_NAME="php-firebird-${EXT_VERSION}-php${php_short//./}-nts-linux-x86_64"
    BUNDLE_DIR="${OUTPUT_DIR}/${BUNDLE_NAME}"
    
    rm -rf "${BUNDLE_DIR}"
    mkdir -p "${BUNDLE_DIR}/lib"
    
    # Copy extension
    cp modules/firebird.so "${BUNDLE_DIR}/"
    
    # Bundle libfbclient and dependencies
    log_info "  Bundling libraries..."
    
    # Copy libfbclient with symlinks
    cp -a ${FB_ROOT}/lib/libfbclient.so* "${BUNDLE_DIR}/lib/"
    
    # Bundle ICU libraries (required by libfbclient)
    for icu_lib in /usr/lib64/libicu*.so*; do
        if [ -f "$icu_lib" ]; then
            cp -a "$icu_lib" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
        fi
    done
    
    # Bundle libtommath and libtomcrypt
    for lib in /usr/lib64/libtommath.so* /usr/lib64/libtomcrypt.so*; do
        if [ -f "$lib" ]; then
            cp -a "$lib" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
        fi
    done
    
    # Bundle re2
    for lib in /usr/lib64/libre2.so*; do
        if [ -f "$lib" ]; then
            cp -a "$lib" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
        fi
    done
    
    # Patch RPATH
    log_info "  Patching RPATH..."
    patchelf --force-rpath --set-rpath '$ORIGIN/lib' "${BUNDLE_DIR}/firebird.so"
    
    for so in "${BUNDLE_DIR}"/lib/*.so*; do
        if [[ -f "$so" && ! -L "$so" ]]; then
            patchelf --force-rpath --set-rpath '$ORIGIN' "$so" 2>/dev/null || true
        fi
    done
    
    # Create documentation
    cat > "${BUNDLE_DIR}/README.txt" << EOF
PHP Firebird Extension ${EXT_VERSION}
====================================

Built with manylinux_2_28 (glibc 2.28) for maximum compatibility.

PHP Version: ${php_short} (NTS)
Architecture: x86_64
Firebird Client: 5.0.x (bundled)

Compatible Linux Distributions:
- Ubuntu 18.10+, 20.04 LTS+
- Debian 10 (Buster)+
- RHEL/CentOS/AlmaLinux/Rocky Linux 8+
- Fedora 29+
- openSUSE Leap 15.1+

Installation:
  EXTDIR=\$(php -r 'echo ini_get("extension_dir");')
  sudo cp -r lib/* \$EXTDIR/../lib/ 2>/dev/null || sudo cp -r lib/* /usr/local/lib/
  sudo cp firebird.so \$EXTDIR/
  echo "extension=firebird.so" | sudo tee /etc/php/${php_short}/mods-available/firebird.ini
  sudo phpenmod firebird
  php -m | grep firebird

Or load directly:
  php -d "extension=\$(pwd)/firebird.so" -m | grep firebird

GitHub: https://github.com/satwareAG/php-firebird
EOF
    
    # Create tarball
    log_info "  Creating tarball..."
    cd "${OUTPUT_DIR}"
    tar -czvf "${BUNDLE_NAME}.tar.gz" "${BUNDLE_NAME}"
    
    # Generate checksum
    sha256sum "${BUNDLE_NAME}.tar.gz" > "${BUNDLE_NAME}.tar.gz.sha256"
    
    # Cleanup bundle directory
    rm -rf "${BUNDLE_DIR}"
    
    log_info "  Created: ${BUNDLE_NAME}.tar.gz"
done

log_info "All bundles built successfully!"
log_info "Output files:"
ls -lh "${OUTPUT_DIR}"/*.tar.gz
SCRIPT
RUN chmod +x /usr/local/bin/build-bundles.sh

# =============================================================================
# Default Command
# =============================================================================

WORKDIR /usr/src/php-firebird

# By default, run the build script
CMD ["/usr/local/bin/build-bundles.sh"]
