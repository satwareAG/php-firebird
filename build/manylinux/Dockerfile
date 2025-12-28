# =============================================================================
# PHP Firebird Extension Build Environment
# Based on manylinux_2_28 (AlmaLinux 8, glibc 2.28)
# =============================================================================
#
# Multi-stage build for optimized layer caching:
#   1. base      - System dependencies and tools
#   2. firebird  - Firebird SDK installation  
#   3. php       - PHP version builds
#   4. builder   - Final build environment
#
# Compatibility: Ubuntu 18.10+, Debian 10+, RHEL 8+, AlmaLinux 8+, Rocky 8+
#
# Usage:
#   docker build -t php-firebird-builder build/manylinux/
#   docker run -v $(pwd):/src php-firebird-builder 8.4 nts x86_64
# =============================================================================

# -----------------------------------------------------------------------------
# Stage 1: Base - System dependencies
# -----------------------------------------------------------------------------
FROM quay.io/pypa/manylinux_2_28_x86_64 AS base

LABEL maintainer="satware AG <support@satware.com>"
LABEL org.opencontainers.image.title="PHP Firebird Build Environment"
LABEL org.opencontainers.image.description="Build environment for precompiled php-firebird extensions"
LABEL org.opencontainers.image.version="1.1.0"
LABEL org.opencontainers.image.source="https://github.com/satwareAG/php-firebird"

# Install build dependencies (cached layer)
RUN --mount=type=cache,target=/var/cache/yum \
    yum install -y \
        # Build essentials
        gcc gcc-c++ make autoconf automake libtool \
        # Extension dependencies
        re2-devel \
        libtommath-devel \
        libtomcrypt-devel \
        libicu-devel \
        ncurses-devel \
        # PHP build dependencies
        libxml2-devel \
        sqlite-devel \
        openssl-devel \
        libcurl-devel \
        libpng-devel \
        libjpeg-devel \
        freetype-devel \
        zlib-devel \
        bzip2-devel \
        readline-devel \
        libxslt-devel \
        libzip-devel \
        oniguruma-devel \
        # Tools
        zstd \
        xz \
    && yum clean all

# Ensure patchelf is available
RUN which patchelf || pip3 install patchelf

# -----------------------------------------------------------------------------
# Stage 2: Firebird SDK installation
# -----------------------------------------------------------------------------
FROM base AS firebird

ARG FB_VERSION=5.0.2
ARG FB_BUILD=1613

# Download and extract Firebird SDK
# Note: Firebird releases have a nested structure:
#   outer.tar.gz -> install.sh, buildroot.tar.gz, manifest.txt
#   buildroot.tar.gz -> actual files under ./opt/firebird/
RUN mkdir -p /opt/firebird/{lib,include,bin} && \
    echo "Downloading Firebird ${FB_VERSION}..." && \
    curl -fsSL "https://github.com/FirebirdSQL/firebird/releases/download/v${FB_VERSION}/Firebird-${FB_VERSION}.${FB_BUILD}-0-linux-x64.tar.gz" \
        -o /tmp/firebird.tar.gz

# Extract nested structure and organize Firebird files
RUN mkdir -p /tmp/fb-outer /tmp/fb-inner && \
    # Extract outer tarball
    tar -xzf /tmp/firebird.tar.gz -C /tmp/fb-outer --strip-components=1 && \
    # Extract inner buildroot tarball
    tar -xzf /tmp/fb-outer/buildroot.tar.gz -C /tmp/fb-inner && \
    # Copy libraries from extracted buildroot
    cp -a /tmp/fb-inner/opt/firebird/lib/libfbclient.so* /opt/firebird/lib/ && \
    # Copy headers
    cp /tmp/fb-inner/opt/firebird/include/ibase.h /opt/firebird/include/ && \
    mkdir -p /opt/firebird/include/firebird && \
    cp /tmp/fb-inner/opt/firebird/include/firebird/*.h /opt/firebird/include/firebird/ 2>/dev/null || true && \
    # Ensure symlinks exist
    cd /opt/firebird/lib && \
    [ ! -L libfbclient.so ] && ln -sf libfbclient.so.2 libfbclient.so || true && \
    # Cleanup
    rm -rf /tmp/fb-outer /tmp/fb-inner /tmp/firebird.tar.gz

# Verify Firebird installation
RUN ls -la /opt/firebird/lib/ && \
    ls -la /opt/firebird/include/ && \
    test -f /opt/firebird/lib/libfbclient.so && \
    echo "Firebird SDK installation verified"

# -----------------------------------------------------------------------------
# Stage 3: PHP version installation
# -----------------------------------------------------------------------------
FROM base AS php

# PHP version configuration
ARG PHP_VERSIONS="8.2.28 8.3.16 8.4.4"

# Create PHP installation directory
RUN mkdir -p /opt/php

# Install PHP versions from php.net
# Using a more reliable approach than shivammathur binaries
RUN for full_version in ${PHP_VERSIONS}; do \
        major_minor="${full_version%.*}"; \
        install_dir="/opt/php/${major_minor}-nts"; \
        \
        echo "=== Installing PHP ${full_version} (NTS) ===" && \
        \
        # Download PHP source
        curl -fsSL "https://www.php.net/distributions/php-${full_version}.tar.xz" \
            -o /tmp/php.tar.xz && \
        \
        # Extract
        mkdir -p /tmp/php-src && \
        tar -xJf /tmp/php.tar.xz -C /tmp/php-src --strip-components=1 && \
        cd /tmp/php-src && \
        \
        # Configure (minimal, CLI only)
        ./configure \
            --prefix="${install_dir}" \
            --enable-cli \
            --disable-cgi \
            --disable-phpdbg \
            --without-pear \
            --enable-shared \
            --enable-mbstring \
            --enable-intl \
            --enable-opcache \
            --with-openssl \
            --with-zlib \
            --with-curl >/dev/null && \
        \
        # Build and install
        make -j$(nproc) >/dev/null && \
        make install >/dev/null 2>&1 && \
        \
        # Verify
        "${install_dir}/bin/php" -v && \
        \
        # Cleanup
        cd / && rm -rf /tmp/php-src /tmp/php.tar.xz && \
        \
        # Build ZTS variant
        echo "=== Installing PHP ${full_version} (ZTS) ===" && \
        zts_dir="/opt/php/${major_minor}-zts" && \
        \
        curl -fsSL "https://www.php.net/distributions/php-${full_version}.tar.xz" \
            -o /tmp/php.tar.xz && \
        mkdir -p /tmp/php-src && \
        tar -xJf /tmp/php.tar.xz -C /tmp/php-src --strip-components=1 && \
        cd /tmp/php-src && \
        \
        ./configure \
            --prefix="${zts_dir}" \
            --enable-cli \
            --disable-cgi \
            --disable-phpdbg \
            --without-pear \
            --enable-shared \
            --enable-mbstring \
            --enable-intl \
            --enable-opcache \
            --enable-zts \
            --with-openssl \
            --with-zlib \
            --with-curl >/dev/null && \
        \
        make -j$(nproc) >/dev/null && \
        make install >/dev/null 2>&1 && \
        \
        "${zts_dir}/bin/php" -v && \
        \
        cd / && rm -rf /tmp/php-src /tmp/php.tar.xz; \
    done

# List installed PHP versions
RUN echo "=== Installed PHP Versions ===" && \
    for dir in /opt/php/*/; do \
        if [ -x "${dir}bin/php" ]; then \
            version=$("${dir}bin/php" -r 'echo PHP_VERSION . " (" . (PHP_ZTS ? "ZTS" : "NTS") . ")";') && \
            echo "  $(basename "$dir"): PHP ${version}"; \
        fi; \
    done

# -----------------------------------------------------------------------------
# Stage 4: Final builder image
# -----------------------------------------------------------------------------
FROM base AS builder

# Copy Firebird from firebird stage
COPY --from=firebird /opt/firebird /opt/firebird

# Copy PHP installations from php stage
COPY --from=php /opt/php /opt/php

# Environment configuration
ENV FB_ROOT=/opt/firebird
ENV PHP_PREFIX=/opt/php
ENV LD_LIBRARY_PATH="/opt/firebird/lib:${LD_LIBRARY_PATH}"
ENV PATH="/opt/firebird/bin:${PATH}"

# Create PHP version switcher script
RUN cat > /usr/local/bin/use-php << 'PHPSWITCH'
#!/bin/bash
# PHP Version Switcher for Build Environment
#
# Usage: use-php VERSION [VARIANT]
#   VERSION: 8.2, 8.3, 8.4
#   VARIANT: nts (default) or zts

VERSION="${1:-8.4}"
VARIANT="${2:-nts}"
PHP_PATH="/opt/php/${VERSION}-${VARIANT}"

if [ ! -d "${PHP_PATH}" ]; then
    # Try without variant suffix
    PHP_PATH="/opt/php/${VERSION}"
fi

if [ -d "${PHP_PATH}" ] && [ -x "${PHP_PATH}/bin/php" ]; then
    export PATH="${PHP_PATH}/bin:${PATH}"
    export PHP_CONFIG="${PHP_PATH}/bin/php-config"
    echo "Switched to PHP ${VERSION} (${VARIANT})"
    php -v | head -1
else
    echo "Error: PHP ${VERSION} (${VARIANT}) not found"
    echo ""
    echo "Available versions:"
    for dir in /opt/php/*/; do
        if [ -x "${dir}bin/php" ]; then
            v=$("${dir}bin/php" -r 'echo PHP_VERSION . " (" . (PHP_ZTS ? "ZTS" : "NTS") . ")";')
            echo "  $(basename "$dir"): ${v}"
        fi
    done
    exit 1
fi
PHPSWITCH
RUN chmod +x /usr/local/bin/use-php

# Create entrypoint wrapper that sets up PHP before build
RUN cat > /usr/local/bin/build-with-php << 'BUILDWRAPPER'
#!/bin/bash
# Build wrapper that sets up PHP environment
set -e

PHP_VERSION="${1:-8.4}"
VARIANT="${2:-nts}"
ARCH="${3:-x86_64}"

# Set up PHP environment
PHP_PATH="/opt/php/${PHP_VERSION}-${VARIANT}"
if [ ! -d "${PHP_PATH}" ]; then
    PHP_PATH="/opt/php/${PHP_VERSION}"
fi

if [ ! -x "${PHP_PATH}/bin/php" ]; then
    echo "Error: PHP ${PHP_VERSION} (${VARIANT}) not found"
    exit 1
fi

export PATH="${PHP_PATH}/bin:${PATH}"
export PHP_CONFIG="${PHP_PATH}/bin/php-config"

echo "=== Build Environment ==="
echo "PHP: $(php -r 'echo PHP_VERSION . " (" . (PHP_ZTS ? "ZTS" : "NTS") . ")";')"
echo "Firebird: ${FB_ROOT}"
echo "Working Dir: $(pwd)"
echo ""

# Run the build script
exec /usr/local/bin/build-precompiled.sh "${PHP_VERSION}" "${VARIANT}" "${ARCH}"
BUILDWRAPPER
RUN chmod +x /usr/local/bin/build-with-php

# Verify installations
RUN echo "=== Verifying Build Environment ===" && \
    echo "Firebird:" && \
    ls -la /opt/firebird/lib/libfbclient.so* && \
    echo "" && \
    echo "PHP Versions:" && \
    ls -la /opt/php/ && \
    echo "" && \
    echo "Tools:" && \
    patchelf --version && \
    echo "" && \
    echo "Build environment ready!"

WORKDIR /src

# Copy build and verification scripts
COPY scripts/build-precompiled.sh /usr/local/bin/
COPY scripts/verify-bundle.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/build-precompiled.sh /usr/local/bin/verify-bundle.sh

# Use the wrapper as entrypoint
ENTRYPOINT ["/usr/local/bin/build-with-php"]
CMD ["8.4", "nts", "x86_64"]
