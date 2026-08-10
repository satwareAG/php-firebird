#!/bin/sh
# =============================================================================
# scripts/test-deb-build.sh
# =============================================================================
#
# Tests compilation + packaging of php-firebird on Ubuntu 24.04 (noble) PHP 8.4
# locally in Docker. Runs the full sequence: FB5 client fetch -> php-firebird
# build -> pdo_fbird build -> RPATH -> dpkg-buildpackage -> install + load test.
#
# This is a COMPILATION + PACKAGING test. It exercises packaging/debian/build.sh
# and packaging/debian/control, producing a real .deb file.
#
# This catches errors that would otherwise only surface in CI (which takes
# 10+ minutes per tag push). Run before tagging any release that touches
# packaging/debian/ or .github/workflows/packages-linux.yml.
#
# Usage:
#   bash scripts/test-deb-build.sh [--php-version 8.4] [--distro noble]
#
# Architecture is auto-detected from the host (x86_64 or aarch64).
#
# Prerequisites: Docker (image auto-pulled).
# Duration: ~5 minutes (FB5 client fetch dominates).
#
# =============================================================================

set -e

PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
PHP_VERSION="8.4"
DISTRO="noble"

# Parse args
while [ $# -gt 0 ]; do
    case "$1" in
        --php-version) PHP_VERSION="$2"; shift 2 ;;
        --distro) DISTRO="$2"; shift 2 ;;
        *) echo "ERROR: Unknown arg: $1" >&2; exit 1 ;;
    esac
done

# Auto-detect architecture
case "$(uname -m)" in
    x86_64) ARCH="x86_64" ;;
    aarch64|arm64) ARCH="aarch64" ;;
    *) echo "ERROR: Unsupported arch: $(uname -m)" >&2; exit 1 ;;
esac

# Map distro to Docker image (mirrors packaging/debian/build-matrix.sh)
case "$DISTRO" in
    bookworm) IMAGE="php:${PHP_VERSION}-cli-bookworm" ;;
    trixie)   IMAGE="php:${PHP_VERSION}-cli-trixie" ;;
    jammy)    IMAGE="ubuntu:22.04" ;;
    noble)    IMAGE="ubuntu:24.04" ;;
    *) echo "ERROR: Unknown distro: $DISTRO" >&2; exit 1 ;;
esac

# Determine if this is Ubuntu (needs Sury PPA)
case "$DISTRO" in
    jammy|noble) IS_UBUNTU=1 ;;
    *) IS_UBUNTU=0 ;;
esac

echo "=== DEB Build Test ($DISTRO PHP $PHP_VERSION $ARCH) ==="
echo "Project: $PROJECT_ROOT"
echo "Docker image: $IMAGE"
echo "Started: $(date)"
echo ""

# Run the full build + install + load test inside a container.
# Exercises the same packaging/debian/build.sh as the CI build-deb job.
# jane: CI pre-fetches FB5 client on the host (GITHUB_TOKEN available) and
#       mounts it read-only. This script fetches inside the container instead.
#       Forward GITHUB_TOKEN if set to avoid GitHub API rate limits on repeats.
docker run --rm \
    -v "$PROJECT_ROOT:/workspace" \
    -w /workspace \
    -e GITHUB_TOKEN="${GITHUB_TOKEN:-}" \
    "$IMAGE" \
    bash -c '
        set -euo pipefail

        PHP_VERSION="'"$PHP_VERSION"'"
        DISTRO="'"$DISTRO"'"
        IS_UBUNTU='"$IS_UBUNTU"'
        ARCH="'"$ARCH"'"

        echo ">>> Installing build dependencies..."
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -qq
        apt-get install -y -qq \
            autoconf \
            automake \
            libtool \
            re2c \
            git \
            pkg-config \
            patchelf \
            dpkg-dev \
            debhelper \
            libicu-dev \
            libxml2-dev \
            libtommath-dev \
            ca-certificates \
            curl \
            gnupg \
            jq \
            >/dev/null 2>&1

        # Install PHP dev headers.
        # Debian: official PHP Docker image has php-dev built in.
        # Ubuntu: install from Sury PPA (packages.sury.org/php).
        if [ "$IS_UBUNTU" = "1" ]; then
            echo ">>> Installing Sury PHP ${PHP_VERSION} (Ubuntu)..."
            CODENAME="$DISTRO"
            curl -sSLo /tmp/keyring.deb https://packages.sury.org/debsuryorg-archive-keyring.deb
            dpkg -i /tmp/keyring.deb >/dev/null 2>&1
            echo "deb [signed-by=/usr/share/keyrings/debsuryorg-archive-keyring.gpg] https://packages.sury.org/php/ ${CODENAME} main" \
                > /etc/apt/sources.list.d/php-sury.list
            apt-get update -qq
            apt-get install -y -qq "php${PHP_VERSION}-dev" "php${PHP_VERSION}-cli" >/dev/null 2>&1
        elif ! command -v "phpize${PHP_VERSION}" >/dev/null 2>&1; then
            echo ">>> Installing PHP ${PHP_VERSION} dev headers..."
            apt-get install -y -qq "php${PHP_VERSION}-dev" "php${PHP_VERSION}-cli" >/dev/null 2>&1
        fi

        echo ">>> PHP version:"
        "php${PHP_VERSION}" -v | head -1

        echo ">>> Building php-firebird .deb..."
        cd /workspace

        # Run the build script (fetches FB5 client, builds extensions, dpkg-buildpackage).
        # jane: pipefail means build.sh failure exits the pipeline non-zero.
        #       We capture output to a log and show tail for context.
        #       The `|| true` prevents set -e from exiting before we can show
        #       the full diagnostic log on failure.
        bash packaging/debian/build.sh \
            --php-version "$PHP_VERSION" \
            --distro "$DISTRO" \
            --arch "$ARCH" \
            --output-dir /tmp/deb-output \
            --verbose 2>&1 | tee /tmp/build.log | tail -20 || {
                echo "FAIL: build.sh failed" >&2
                echo "--- Last 30 lines of build log ---" >&2
                tail -30 /tmp/build.log >&2
                exit 1
            }

        # Find the .deb file
        DEB_FILE=$(find /tmp/deb-output -name "*.deb" ! -name "*dbgsym*" -type f | head -1)
        if [ -z "$DEB_FILE" ]; then
            echo "FAIL: no .deb file produced" >&2
            echo "--- Last 30 lines of build log ---" >&2
            tail -30 /tmp/build.log >&2
            exit 1
        fi
        echo ">>> .deb: $(basename "$DEB_FILE")"

        echo ">>> Installing .deb..."
        dpkg -i "$DEB_FILE" 2>&1 || apt-get install -f -y -qq 2>&1

        echo ">>> Load test..."
        VERSION=$("php${PHP_VERSION}" \
            -d extension=firebird.so \
            -d extension=pdo_fbird.so \
            -r "echo phpversion(\"firebird\");" 2>/tmp/load-error.log || echo "")

        if [ -z "$VERSION" ]; then
            echo "FAIL: extension did not load" >&2
            echo "--- PHP error output ---" >&2
            cat /tmp/load-error.log >&2
            exit 1
        fi

        echo ""
        echo "=== PASS: php-firebird $VERSION loaded on $DISTRO PHP $PHP_VERSION ==="
        echo ""
        echo "Package: $(basename "$DEB_FILE")"
        echo "Size: $(ls -lh "$DEB_FILE" | awk "{print \$5}")"
    '

echo ""
echo "=== DEB Build Test PASSED ==="
echo "Finished: $(date)"
