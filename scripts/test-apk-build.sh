#!/bin/sh
# =============================================================================
# scripts/test-apk-build.sh
# =============================================================================
#
# Tests compilation of php-firebird on Alpine 3.21 (musl) locally in Docker.
# Runs the full sequence: Firebird client build -> php-firebird build ->
# pdo_fbird build -> extension load test.
#
# This is a COMPILATION test, not a full abuild test. It does NOT exercise
# the APKBUILD build()/package() functions, checksum verification, or INI
# file installation. For full abuild testing, run in CI via tag push.
#
# This catches compilation errors that would otherwise only surface in CI
# (which takes 10+ minutes per tag push). Run before tagging any release
# that touches packaging/apk/ or .github/workflows/packages-linux.yml.
#
# Usage:
#   bash scripts/test-apk-build.sh
#
# Prerequisites: Docker, alpine:3.21 image (auto-pulled).
# Duration: ~10 minutes (Firebird source build dominates).
#
# =============================================================================

set -e

PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)

echo "=== APK Compilation Test (Alpine 3.21 musl) ==="
echo "Project: $PROJECT_ROOT"
echo "Started: $(date)"
echo ""

# Run the full build + load test inside an Alpine container.
# Mirrors the compilation steps in the CI workflow's build-apk job.
docker run --rm \
    -v "$PROJECT_ROOT:/workspace" \
    -w /workspace \
    alpine:3.21 \
    sh -c '
        set -euo pipefail

        echo ">>> Installing build dependencies..."
        apk add --no-cache \
            build-base \
            autoconf \
            automake \
            libtool \
            re2c \
            git \
            icu-dev \
            zlib-dev \
            ncurses-dev \
            php84-dev \
            php84-pdo \
            patchelf

        echo ">>> Building Firebird client (musl)..."
        FB_ROOT=/tmp/fbclient FB_VERSION=5.0.4 \
            sh packaging/apk/firebird-client-musl-build.sh 2>&1 | tee /tmp/fb-build.log | tail -5

        # Verify libs
        if [ ! -f /tmp/fbclient/lib/libfbclient.so ]; then
            echo "FAIL: libfbclient.so not built" >&2
            echo "--- Last 20 lines of build log ---" >&2
            tail -20 /tmp/fb-build.log >&2
            exit 1
        fi
        echo ">>> Libraries: $(ls /tmp/fbclient/lib/)"

        # Verify NEEDED dependencies are present
        TOMMATH_NEEDED=$(readelf -d /tmp/fbclient/lib/libfbclient.so | grep -c "libtommath" || true)
        if [ "$TOMMATH_NEEDED" -gt 0 ]; then
            if [ ! -f /tmp/fbclient/lib/libtommath.so ]; then
                echo "FAIL: libfbclient needs libtommath.so but it was not copied" >&2
                exit 1
            fi
        fi

        echo ">>> Building php-firebird extension..."
        cd /workspace
        phpize84 --clean
        phpize84
        CFLAGS="-I/tmp/fbclient/include -I/tmp/fbclient/include/firebird" \
        CXXFLAGS="-I/tmp/fbclient/include -I/tmp/fbclient/include/firebird" \
        LDFLAGS="-L/tmp/fbclient/lib -Wl,-rpath-link,/tmp/fbclient/lib" \
            ./configure \
            --with-php-config=/usr/bin/php-config84 \
            --with-firebird=/tmp/fbclient 2>&1 | tee /tmp/main-configure.log | tail -3
        make -j"$(nproc)" 2>&1 | tee /tmp/main-build.log | tail -3

        if [ ! -f modules/firebird.so ]; then
            echo "FAIL: firebird.so not built" >&2
            echo "--- Last 20 lines of build log ---" >&2
            tail -20 /tmp/main-build.log >&2
            exit 1
        fi

        echo ">>> Building pdo_fbird extension..."
        cd pdo_fbird
        phpize84 --clean
        phpize84
        CFLAGS="-I/tmp/fbclient/include -I/tmp/fbclient/include/firebird" \
        CXXFLAGS="-I/tmp/fbclient/include -I/tmp/fbclient/include/firebird" \
        LDFLAGS="-L/tmp/fbclient/lib -Wl,-rpath-link,/tmp/fbclient/lib" \
            ./configure \
            --with-php-config=/usr/bin/php-config84 \
            --with-firebird=/tmp/fbclient 2>&1 | tee /tmp/pdo-configure.log | tail -3
        make -j"$(nproc)" 2>&1 | tee /tmp/pdo-build.log | tail -3

        if [ ! -f modules/pdo_fbird.so ]; then
            echo "FAIL: pdo_fbird.so not built" >&2
            echo "--- Last 20 lines of build log ---" >&2
            tail -20 /tmp/pdo-build.log >&2
            exit 1
        fi

        echo ">>> Load test..."
        export LD_LIBRARY_PATH=/tmp/fbclient/lib
        VERSION=$(php84 \
            -d extension=/workspace/modules/firebird.so \
            -d extension=/workspace/pdo_fbird/modules/pdo_fbird.so \
            -r "echo phpversion(\"firebird\");" 2>/tmp/load-error.log || echo "")

        if [ -z "$VERSION" ]; then
            echo "FAIL: extension did not load" >&2
            echo "--- PHP error output ---" >&2
            cat /tmp/load-error.log >&2
            exit 1
        fi

        echo ""
        echo "=== PASS: php-firebird $VERSION loaded on Alpine 3.21 (musl) ==="
    '

echo ""
echo "=== APK Compilation Test PASSED ==="
echo "Finished: $(date)"
