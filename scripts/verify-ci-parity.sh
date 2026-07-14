#!/usr/bin/env bash
# =============================================================================
# verify-ci-parity.sh - Verify local Docker client matches CI client
# =============================================================================
# Ensures the local Docker container uses the same Firebird client library
# as the CI environment. This prevents the "passes locally, fails in CI"
# scenario caused by FB_API_VER mismatch (FB3 apt vs FB4 official tarball).
#
# Usage: bash scripts/verify-ci-parity.sh [container_name]
# Default: php84-dev
#
# Exit: 0 = parity confirmed, 1 = mismatch found
# =============================================================================
set -euo pipefail

CONTAINER="${1:-php84-dev}"
DOCKER_DIR="$(cd "$(dirname "$0")/.." && pwd)/docker"

echo "=== CI Parity Check ==="
echo "Container: $CONTAINER"
echo ""

# Check that the container is running
cd "$DOCKER_DIR"
export CURRENT_UID=$(id -u)
export CURRENT_GID=$(id -g)

if [ -z "$(docker compose ps -q "$CONTAINER" 2>/dev/null)" ]; then
    echo "Starting container $CONTAINER..."
    docker compose up -d "$CONTAINER" 2>/dev/null
fi

# Check FB_API_VER inside the container
echo "--- Firebird Client ---"
FB_API_VER=$(docker compose exec -T "$CONTAINER" bash -c '
cpp -dM -I/opt/firebird/include -include ibase.h /dev/null 2>/dev/null | grep FB_API_VER | awk "{print \$3}"
' 2>/dev/null || echo "UNKNOWN")

FB_VERSION=$(docker compose exec -T "$CONTAINER" bash -c '
strings /opt/firebird/lib/libfbclient.so 2>/dev/null | grep "LI-V" | head -1
' 2>/dev/null || echo "UNKNOWN")

if [ -z "$FB_API_VER" ] || [ "$FB_API_VER" = "UNKNOWN" ]; then
    echo "FB_API_VER: NOT FOUND (client headers not installed at /opt/firebird)"
    echo ""
    echo "ERROR: Container $CONTAINER does not have the official Firebird client."
    echo "It may be using the Debian apt 'firebird-dev' package (FB3 client)."
    echo "This means FB_API_VER < 40 and all DECFLOAT/INT128/timezone code is"
    echo "compiled out. Tests will silently SKIP instead of running."
    echo ""
    echo "Fix: Rebuild the Docker image:"
    echo "  cd docker && docker compose build --no-cache $CONTAINER"
    exit 1
fi

echo "FB_API_VER: $FB_API_VER"
echo "Client version: $FB_VERSION"

# CI expects FB_API_VER >= 40 for the default containers (they connect to firebird40)
EXPECTED_MIN_API=40
if [ "$FB_API_VER" -lt "$EXPECTED_MIN_API" ]; then
    echo ""
    echo "ERROR: FB_API_VER=$FB_API_VER but CI uses FB_API_VER=$EXPECTED_MIN_API+"
    echo "The local container uses an older Firebird client than CI."
    echo "DECFLOAT, INT128, and timezone code paths will NOT be compiled."
    echo ""
    echo "Fix: Rebuild the Docker image:"
    echo "  cd docker && docker compose build --no-cache $CONTAINER"
    exit 1
fi

# Check that FIREBIRD_DB_DIR is set in CI config
echo ""
echo "--- CI Environment Variables ---"
if grep -q 'FIREBIRD_DB_DIR' "$DOCKER_DIR/../.github/workflows/ci.yml"; then
    echo "FIREBIRD_DB_DIR: set in ci.yml (good)"
else
    echo "FIREBIRD_DB_DIR: NOT SET in ci.yml"
    echo ""
    echo "WARNING: Without FIREBIRD_DB_DIR, the procedural path (firebird.inc)"
    echo "uses the same database as the PDO path (pdo_fbird.inc). This causes"
    echo "metadata lock conflicts when both paths create tables with the same name."
    echo ""
    echo "Fix: Add 'FIREBIRD_DB_DIR: /tmp' to the env: section of ci.yml"
    exit 1
fi

# Check --set-timeout in CI config
if grep -q '\-\-set-timeout' "$DOCKER_DIR/../.github/workflows/ci.yml"; then
    echo "--set-timeout: set in ci.yml (good)"
else
    echo "--set-timeout: NOT SET in ci.yml"
    echo ""
    echo "WARNING: Without --set-timeout, hanging tests take 60s each instead of 15s."
    echo "This can cause CI to exceed the 30-minute job timeout."
    exit 1
fi

echo ""
echo "=== Parity Confirmed ==="
echo "Local Docker client matches CI expectations."
exit 0
