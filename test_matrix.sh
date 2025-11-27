#!/bin/bash
set -e

# Modern implementation of test runner across PHP versions
# Usage: ./test_matrix.sh [container_name] [test_files...]
# Example: ./test_matrix.sh php81-dev tests/ibase_blob_001.phpt tests/ibase_blob_002.phpt
# Example (all versions, specific test): ./test_matrix.sh "" tests/ibase_blob_001.phpt

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

PROJECT_ROOT=$(pwd)
DOCKER_DIR="$PROJECT_ROOT/docker"

echo -e "${BLUE}=== PHP Firebird Test Matrix ===${NC}"

# 1. Check Prerequisites
if ! command -v docker >/dev/null 2>&1; then
    echo -e "${RED}Error: docker is not installed.${NC}"
    exit 1
fi

# 2. Ensure Environment
echo -e "${BLUE}>> Infrastructure Check${NC}"
cd "$DOCKER_DIR"

# Export UID/GID for docker compose substitution
export CURRENT_UID=$(id -u)
export CURRENT_GID=$(id -g)

# Bring up containers (ensure up-to-date)
# We use --wait to ensure healthchecks pass before testing (if configured)
# But php-dev containers typically default to running state.
if ! docker compose up -d; then
    echo -e "${RED}Failed to start Docker environment.${NC}"
    exit 1
fi

# 3. Define Targets
if [ -n "$1" ]; then
    TARGETS=("$1")
else
    TARGETS=("php81-dev" "php82-dev" "php83-dev" "php84-dev" "php85-dev")
fi

# Define Test Targets (optional)
TEST_TARGETS=""
if [ -n "$2" ]; then
    # Capture all arguments starting from position 2
    TEST_TARGETS="${@:2}"
    echo "Targeting specific tests: $TEST_TARGETS"
fi

# 4. Execute Matrix
for CONTAINER in "${TARGETS[@]}"; do
    echo -e "\n${BLUE}>> Testing Target: $CONTAINER${NC}"

    # Verify container state
    if [ -z "$(docker compose ps -q $CONTAINER)" ]; then
        echo -e "${YELLOW}Container $CONTAINER is not running. Starting...${NC}"
        docker compose up -d $CONTAINER
        if [ -z "$(docker compose ps -q $CONTAINER)" ]; then
            echo -e "${RED}Failed to start container $CONTAINER.${NC}"
            echo "Available services:"
            docker compose ps --services
            exit 1
        fi
    fi

    # Fix permissions as root first (in case previous runs left root-owned files)
    echo "Fixing permissions..."
    docker compose exec -u root "$CONTAINER" chown -R $CURRENT_UID:$CURRENT_GID /ext

    # Run Build & Test in single session
    # Optimizes overhead and ensures clean build state
    echo "Running build and test suite (Target: ${TEST_TARGET:-ALL})..."

    # Construct command with optional target
    CMD="/ext/docker/scripts/build-extension.sh && /ext/docker/scripts/test-extension.sh"
    if [ -n "$TEST_TARGETS" ]; then
        CMD="$CMD $TEST_TARGETS"
    fi

    if docker compose exec "$CONTAINER" bash -c "$CMD"; then
        echo -e "${GREEN}✓ $CONTAINER passed${NC}"
    else
        echo -e "${RED}✗ $CONTAINER failed${NC}"
        exit 1
    fi
done

echo -e "\n${GREEN}=== All Targeted Versions Passed ===${NC}"
