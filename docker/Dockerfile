# For building linux-x64 release binaries
# PHP 8.2+ minimum (no 7.x, 8.0, or 8.1 support)
FROM debian:bookworm@sha256:30482e873082e906a4908c10529180aefb6f77620aea7404b909829fadc5d168

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential autoconf bison re2c wget tar git patchelf libtommath1

# Download php (PHP 8.2+ only)
ARG php_vers="8.2.32 8.3.32 8.4.23 8.5.8"
RUN --mount=type=cache,target=/usr/src \
    cd /usr/src && \
    set -e && \
    for php in $php_vers; do \
        if [ ! -d php-$php ]; then \
            git clone --depth 1 --branch php-$php https://github.com/php/php-src.git php-$php; \
        fi \
    done

# Build php
RUN --mount=type=cache,target=/usr/src \
    for php in $php_vers; do \
        php_short=${php%.*} && \
        cd /usr/src/php-$php && \
        ./buildconf --force && \
        ./configure \
            --prefix=/opt/php-$php_short \
            --disable-all --disable-cgi --enable-cli --build=x86_64-linux-gnu --host=x86_64-linux-gnu && \
        make -j$(nproc) && make install; \
    done

# Download Firebird-5.0.3
ARG firebird=Firebird-5.0.3.1683-0-linux-x64
RUN --mount=type=cache,target=/usr/src \
    cd /usr/src && \
    if [ ! -d $firebird ]; then \
        wget https://github.com/FirebirdSQL/firebird/releases/download/v5.0.3/$firebird.tar.gz; \
    fi && \
    tar xf $firebird.tar.gz && \
    mkdir -p /opt/$firebird && \
    tar xf $firebird/buildroot.tar.gz -C /opt/$firebird

# Clone php-firebird from satwareAG
ARG branch=main
RUN --mount=type=cache,target=/usr/src \
    cd /usr/src && \
    if [ -d php-firebird ]; then \
        cd php-firebird && git fetch && git checkout $branch && git pull; \
    else \
        git clone --depth 1 --branch "$branch" "https://github.com/satwareAG/php-firebird.git"; \
    fi

# Build php-firebird extension
RUN --mount=type=cache,target=/usr/src \
    for php in $php_vers; do \
        php_short=${php%.*} && \
        cd /usr/src/php-firebird && \
        (make clean || true) && \
        (/opt/php-$php_short/bin/phpize --clean || true) && \
        /opt/php-$php_short/bin/phpize && \
        ./configure \
            --with-php-config=/opt/php-$php_short/bin/php-config \
            --with-firebird=/opt/$firebird/opt/firebird || (cat config.log && false) && \
        make -j$(nproc) && \
        patchelf --remove-rpath ./modules/firebird.so && \
        cp ./modules/firebird.so /opt/php${php_short}-firebird-5.0.3-linux.so; \
    done
