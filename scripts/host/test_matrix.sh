#!/bin/bash
set -e

# Modern implementation of test runner across PHP versions
# Usage: ./test_matrix.sh [container_name] [firebird_server] [test_files...]
# Example: ./test_matrix.sh php81-dev
# Example: ./test_matrix.sh php85-fb5-dev firebird50
# Example: ./test_matrix.sh php81-dev "" tests/fbird_blob_001.phpt
# Example (all versions, specific test): ./test_matrix.sh "" "" tests/fbird_blob_001.phpt
#
# Arguments:
#   container_name   - PHP container to test (e.g., php81-dev, php85-fb5-dev)
#   firebird_server  - Target Firebird server (firebird25, firebird30, firebird40, firebird50)
#   test_files       - Optional specific test files to run

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
if ! docker compose up -d --remove-orphans; then
    echo -e "${RED}Failed to start Docker environment.${NC}"
    exit 1
fi

# 3. Define Targets
if [ -n "$1" ]; then
    TARGETS=("$1")
else
    # Test all PHP containers including special client library variants:
    # - Standard containers (php8x-dev): Use apt firebird-dev (Firebird 4.x client)
    # - php84-fb3-dev: Firebird 3.0.12 client for FB 2.5/3.0 server compatibility
    # - php85-fb5-dev: Firebird 5.x client for latest features
    TARGETS=("php81-dev" "php82-dev" "php83-dev" "php84-dev" "php84-fb3-dev" "php85-dev" "php85-fb5-dev")
fi

# 4. Parse Firebird Server Target (optional second argument)
FIREBIRD_SERVER=""
if [ -n "$2" ]; then
    case "$2" in
        firebird25|firebird30|firebird40|firebird50)
            FIREBIRD_SERVER="$2"
            echo -e "${BLUE}>> Firebird Server Target: $FIREBIRD_SERVER${NC}"
            ;;
        "")
            # Empty string - use container default
            ;;
        *)
            echo -e "${RED}Error: Invalid Firebird server '$2'${NC}"
            echo "Valid servers: firebird25, firebird30, firebird40, firebird50"
            echo "Usage: $0 [container] [firebird_server] [test_files...]"
            exit 1
            ;;
    esac
fi

# 5. Define Test Targets (optional, starting from position 3)
TEST_TARGETS=""
if [ -n "$3" ]; then
    # Capture all arguments starting from position 3
    TEST_TARGETS="${@:3}"
    echo -e "${BLUE}>> Targeting specific tests: $TEST_TARGETS${NC}"
fi

# 6. Execute Matrix
FAILED_CONTAINERS=()
PASSED_CONTAINERS=()

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
            FAILED_CONTAINERS+=("$CONTAINER (failed to start)")
            continue
        fi
    fi

    # Fix permissions as root first (in case previous runs left root-owned files)
    echo "Fixing permissions..."
    docker compose exec -u root "$CONTAINER" chown -R $CURRENT_UID:$CURRENT_GID /ext

    # Build environment variable options for docker exec
    ENV_OPTS=""
    if [ -n "$FIREBIRD_SERVER" ]; then
        # When overriding FIREBIRD_HOST we also force FIREBIRD_DB_DIR to /tmp.
        # Reason: some PHP containers mount /firebird volumes that are NOT present
        # (or writable) in the selected Firebird server container, which breaks
        # CREATE DATABASE during SKIPIF/init_db().
        ENV_OPTS="-e FIREBIRD_HOST=$FIREBIRD_SERVER -e FIREBIRD_DB_DIR=/tmp"
        echo "Using Firebird server: $FIREBIRD_SERVER (FIREBIRD_DB_DIR=/tmp)"
    fi

    # Run Build & Test in single session
    # Optimizes overhead and ensures clean build state
    echo "Running build and test suite (Server: ${FIREBIRD_SERVER:-default}, Tests: ${TEST_TARGETS:-ALL})..."

    # Construct command with optional target
    CMD="/ext/scripts/container/build.sh && /ext/scripts/container/test.sh"
    if [ -n "$TEST_TARGETS" ]; then
        CMD="$CMD $TEST_TARGETS"
    fi

    if docker compose exec $ENV_OPTS "$CONTAINER" bash -c "$CMD"; then
        echo -e "${GREEN}✓ $CONTAINER passed${NC}"
        PASSED_CONTAINERS+=("$CONTAINER")
    else
        echo -e "${RED}✗ $CONTAINER failed${NC}"
        FAILED_CONTAINERS+=("$CONTAINER")
    fi
done

echo -e "\n${BLUE}=== Test Matrix Summary ===${NC}"
echo -e "Passed (${#PASSED_CONTAINERS[@]}): ${PASSED_CONTAINERS[*]}"
echo -e "Failed (${#FAILED_CONTAINERS[@]}): ${FAILED_CONTAINERS[*]}"

if [ ${#FAILED_CONTAINERS[@]} -eq 0 ]; then
    echo -e "\n${GREEN}=== All Targeted Versions Passed ===${NC}"
    exit 0
else
    echo -e "\n${RED}=== Some Versions Failed ===${NC}"
    exit 1
fi
