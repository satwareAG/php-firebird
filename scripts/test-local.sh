#!/bin/bash
# scripts/test-local.sh
# Replicates the CI test environment locally using Docker.
# ~30s turnaround vs 20+ minutes for CI.
#
# Usage:
#   ./scripts/test-local.sh [php_version] [fb_version] [test_path]
#
# Examples:
#   ./scripts/test-local.sh                    # PHP 8.4, FB 3, all tests
#   ./scripts/test-local.sh 8.4 3              # PHP 8.4, FB 3, all tests
#   ./scripts/test-local.sh 8.4 3 tests/pdo_fbird/conformance/  # conformance only
#   ./scripts/test-local.sh 8.5 5              # PHP 8.5, FB 5

set -euo pipefail

PHP_VERSION="${1:-8.4}"
FB_MAJOR="${2:-3}"
TEST_PATH="${3:-tests/}"
NETWORK_NAME="fb-test-net-$$"
FB_CONTAINER="fb-server-$$"
PHP_CONTAINER="fb-test-runner-$$"

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"

cleanup() {
    echo "=== Cleaning up ==="
    docker rm -f "$FB_CONTAINER" 2>/dev/null || true
    docker rm -f "$PHP_CONTAINER" 2>/dev/null || true
    docker network rm "$NETWORK_NAME" 2>/dev/null || true
}
trap cleanup EXIT

echo "=== Local CI Test Simulation ==="
echo "PHP: ${PHP_VERSION}  Firebird: ${FB_MAJOR}  Tests: ${TEST_PATH}"
echo ""

# 1. Create Docker network
docker network create "$NETWORK_NAME" 2>/dev/null || true

# 2. Start Firebird container (matches CI service container)
echo "=== Starting Firebird ${FB_MAJOR} container ==="
docker run -d --name "$FB_CONTAINER" \
    --network "$NETWORK_NAME" \
    --network-alias firebird \
    -e "ISC_PASSWORD=masterkey" \
    -e "FIREBIRD_DATABASE=test.fdb" \
    -e "FIREBIRD_USE_LEGACY_AUTH=Enabled" \
    firebirdsql/firebird:"${FB_MAJOR}"

# 3. Wait for Firebird to be ready
echo "=== Waiting for Firebird ==="
for i in $(seq 1 30); do
    if docker exec "$FB_CONTAINER" bash -c '</dev/tcp/localhost/3050' 2>/dev/null; then
        echo "Firebird is ready (attempt ${i}/30)"
        sleep 3
        break
    fi
    echo "  Waiting (${i}/30)..."
    sleep 2
done

if ! docker exec "$FB_CONTAINER" bash -c '</dev/tcp/localhost/3050' 2>/dev/null; then
    echo "ERROR: Firebird did not start"
    docker logs "$FB_CONTAINER"
    exit 1
fi

# 4. Run tests in PHP container
echo "=== Running tests in php:${PHP_VERSION}-cli ==="
docker run --rm \
    --name "$PHP_CONTAINER" \
    --network "$NETWORK_NAME" \
    -v "${REPO_DIR}:/app" \
    -w /app \
    -e "FIREBIRD_HOST=firebird" \
    -e "FIREBIRD_DB_PATH=/firebird/data/test.fdb" \
    -e "NO_INTERACTION=1" \
    php:"${PHP_VERSION}"-cli \
    bash -c '
        set -euo pipefail
        export DEBIAN_FRONTEND=noninteractive

        echo "--- Installing system dependencies ---"
        apt-get update -qq
        apt-get install -y -qq --no-install-recommends \
            build-essential autoconf libtool pkg-config \
            jq curl wget ca-certificates \
            libtommath1 libncurses6 netcat-openbsd \
            >/dev/null 2>&1

        # Library compatibility symlinks
        if [ -f /lib/x86_64-linux-gnu/libtommath.so.1 ]; then
            ln -sf /lib/x86_64-linux-gnu/libtommath.so.1 /lib/x86_64-linux-gnu/libtommath.so.0
        fi

        echo "--- Resolving and downloading Firebird client ---"
        chmod +x scripts/get-latest-firebird.sh
        eval "$(./scripts/get-latest-firebird.sh '"${FB_MAJOR}"')"
        echo "Firebird client: ${FB_VERSION}"

        curl -fsSL --retry 3 "${FB_URL}" -o "/tmp/${FB_TARBALL}"
        cd /tmp && tar xzf "${FB_TARBALL}" && cd "${FB_EXTRACT_DIR}"
        tar xzf buildroot.tar.gz

        FB_CLIENT_DIR="/opt/firebird-client"
        mkdir -p "${FB_CLIENT_DIR}/include" "${FB_CLIENT_DIR}/lib"
        cp -a opt/firebird/include/* "${FB_CLIENT_DIR}/include/" 2>/dev/null || true
        cp -a opt/firebird/lib/* "${FB_CLIENT_DIR}/lib/" 2>/dev/null || true

        # Install to system paths
        for libfile in "${FB_CLIENT_DIR}/lib/"*.so*; do
            [ -f "$libfile" ] && [ ! -L "$libfile" ] && cp "$libfile" /usr/lib/
        done
        if ls /usr/lib/libfbclient.so.*.*.* 1>/dev/null 2>&1; then
            FBCLIENT_REAL=$(ls /usr/lib/libfbclient.so.*.*.* | head -1)
            FBCLIENT_NAME=$(basename "$FBCLIENT_REAL")
            ln -sf "$FBCLIENT_NAME" /usr/lib/libfbclient.so.2
            ln -sf libfbclient.so.2 /usr/lib/libfbclient.so
        fi
        if ls /usr/lib/libtomcrypt.so.*.*.* 1>/dev/null 2>&1; then
            TOMCRYPT_REAL=$(ls /usr/lib/libtomcrypt.so.*.*.* | head -1)
            TOMCRYPT_NAME=$(basename "$TOMCRYPT_REAL")
            ln -sf "$TOMCRYPT_NAME" /usr/lib/libtomcrypt.so.1
            ln -sf libtomcrypt.so.1 /usr/lib/libtomcrypt.so
        fi
        cp -a "${FB_CLIENT_DIR}/include"/* /usr/include/ 2>/dev/null || true
        echo -e "${FB_CLIENT_DIR}/lib\n/usr/local/lib\n/usr/lib" > /etc/ld.so.conf.d/firebird-client.conf
        ldconfig

        # Firebird client auth config
        mkdir -p /etc/firebird
        echo -e "AuthClient = Srp256, Srp, Legacy_Auth\nWireCrypt = Enabled" > /etc/firebird/firebird.conf

        echo "--- Building firebird extension ---"
        cd /app
        phpize --clean 2>/dev/null || true
        find . -name '*.lo' -o -name '*.dep' -o -name '*.o' | xargs rm -f 2>/dev/null || true
        rm -rf autom4te.cache modules
        phpize
        find . -name '*.dep' -delete 2>/dev/null || true
        ./configure --with-firebird="${FB_CLIENT_DIR}" 2>&1 | tail -3
        make -j"$(nproc)" 2>&1 | tail -3

        FIREBIRD_SO="$(pwd)/modules/firebird.so"

        echo "--- Building pdo_fbird extension ---"
        cd pdo_fbird
        phpize --clean 2>/dev/null || true
        find . -name '*.lo' -o -name '*.dep' -o -name '*.o' | xargs rm -f 2>/dev/null || true
        rm -rf autom4te.cache modules
        phpize
        find . -name '*.dep' -delete 2>/dev/null || true
        ./configure --with-pdo-fbird --with-firebird="${FB_CLIENT_DIR}" 2>&1 | tail -3
        make -j"$(nproc)" 2>&1 | tail -3

        PDO_FBIRD_SO="$(pwd)/modules/pdo_fbird.so"

        cd /app

        echo "--- Verifying extension loading ---"
        php -d extension="${FIREBIRD_SO}" -d extension="${PDO_FBIRD_SO}" -m | grep -iE "firebird|PDO" || true
        echo ""
        php -d extension="${FIREBIRD_SO}" -d extension="${PDO_FBIRD_SO}" -r "
            echo \"firebird loaded: \" . (extension_loaded(\"firebird\") ? \"yes\" : \"no\") . \"\n\";
            echo \"pdo_fbird loaded: \" . (extension_loaded(\"pdo_fbird\") ? \"yes\" : \"no\") . \"\n\";
            echo \"PDO drivers: \" . implode(\", \", PDO::getAvailableDrivers()) . \"\n\";
        "

        echo "--- Running tests: '"${TEST_PATH}"' ---"
        echo ""
        # jane: use || to capture exit code because set -e would exit before reaching $?
        TEST_PHP_EXECUTABLE=$(which php) \
        php run-tests.php \
            -d extension="${FIREBIRD_SO}" \
            -d extension="${PDO_FBIRD_SO}" \
            -p "$(which php)" \
            "'"$TEST_PATH"'" || TEST_RESULT=$?

        if [ "${TEST_RESULT:-0}" -ne 0 ]; then
            echo ""
            echo "=== TEST FAILURES DETECTED (exit code ${TEST_RESULT}) ==="
        else
            echo ""
            echo "=== ALL TESTS PASSED ==="
        fi
        exit ${TEST_RESULT:-0}
    '

RESULT=$?
echo ""
if [ $RESULT -eq 0 ]; then
    echo "SUCCESS: All tests passed"
else
    echo "FAILURE: Some tests failed (exit code ${RESULT})"
fi
exit $RESULT
