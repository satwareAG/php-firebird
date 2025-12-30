# =============================================================================
# PHP Firebird Extension - Manylinux Builder Image
# =============================================================================
#
# Build environment for precompiled PHP extensions with bundled libraries.
# Based on manylinux_2_28 (AlmaLinux 8, glibc 2.28) for maximum compatibility.
#
# Usage:
#   docker build -t php-firebird-builder build/manylinux/
#   docker run --rm -v $(pwd):/src php-firebird-builder 8.4 nts x86_64
#
# =============================================================================

FROM quay.io/pypa/manylinux_2_28_x86_64

LABEL maintainer="satware AG <dev@satware.com>"
LABEL description="PHP Firebird Extension Builder (manylinux_2_28)"
LABEL org.opencontainers.image.source="https://github.com/satwareAG/php-firebird"

# =============================================================================
# Install Build Dependencies
# =============================================================================

RUN dnf -y update && \
    dnf -y install \
        # Build essentials
        autoconf \
        automake \
        bison \
        flex \
        gcc \
        gcc-c++ \
        git \
        libtool \
        make \
        pkgconfig \
        re2c \
        # PHP build dependencies
        bzip2-devel \
        libcurl-devel \
        libicu-devel \
        libxml2-devel \
        oniguruma-devel \
        openssl-devel \
        readline-devel \
        sqlite-devel \
        zlib-devel \
        # Firebird client dependencies
        libtommath-devel \
        libtomcrypt-devel \
        re2-devel \
        ncurses-devel \
        # ELF manipulation
        patchelf \
        # Utilities
        wget \
        curl \
        tar \
        xz \
    && dnf clean all

# =============================================================================
# Install Multiple PHP Versions
# =============================================================================

# PHP version installation script
COPY install-php.sh /usr/local/bin/install-php.sh
RUN chmod +x /usr/local/bin/install-php.sh

# Install PHP versions (NTS and ZTS variants)
# Using shivammathur/php-builder binaries for consistency
ENV PHP_VERSIONS="8.1 8.2 8.3 8.4"

RUN for ver in $PHP_VERSIONS; do \
        /usr/local/bin/install-php.sh "$ver" nts; \
        /usr/local/bin/install-php.sh "$ver" zts; \
    done

# Set default PHP
RUN ln -sf /opt/php/8.4-nts/bin/php /usr/local/bin/php && \
    ln -sf /opt/php/8.4-nts/bin/phpize /usr/local/bin/phpize && \
    ln -sf /opt/php/8.4-nts/bin/php-config /usr/local/bin/php-config

# =============================================================================
# Install Firebird SDK
# =============================================================================

ENV FB_VERSION=5.0.2
ENV FB_ROOT=/opt/firebird

RUN mkdir -p ${FB_ROOT} && \
    cd /tmp && \
    curl -L -o firebird.tar.gz \
        "https://github.com/FirebirdSQL/firebird/releases/download/v${FB_VERSION}/Firebird-${FB_VERSION}.1-linux-x64.tar.gz" && \
    tar xzf firebird.tar.gz && \
    cd Firebird-* && \
    # Copy libraries and headers
    mkdir -p ${FB_ROOT}/{lib,include} && \
    cp -a lib/* ${FB_ROOT}/lib/ && \
    cp -a include/* ${FB_ROOT}/include/ && \
    # Create necessary symlinks
    cd ${FB_ROOT}/lib && \
    ln -sf libfbclient.so.2 libfbclient.so && \
    # Cleanup
    rm -rf /tmp/firebird* /tmp/Firebird*

# Add Firebird to library path (for build, not runtime - bundle uses $ORIGIN)
ENV LD_LIBRARY_PATH="${FB_ROOT}/lib:${LD_LIBRARY_PATH}"

# =============================================================================
# Entrypoint Script
# =============================================================================

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

WORKDIR /src

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD ["8.4", "nts", "x86_64"]
